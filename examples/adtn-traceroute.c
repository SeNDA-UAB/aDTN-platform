#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>

#include "platform/common/include/constants.h"
#include "platform/common/include/config.h"
#include "platform/common/include/shm.h"
#include "platform/common/include/log.h"
#include "platform/common/include/utils.h"
#include "platform/common/include/queue.h"
#include "platform/common/include/bundle.h"
#include "platform/common/include/utlist.h"

#include "lib/api/include/adtn.h"

#define DEFAULT_LIFETIME 30
#define DEFAULT_PAYLOAD_SIZE 64
#define DEFAULT_TIMEOUT 5
#define SNPRINTF(...)                                               \
	do{                                                             \
		int r = snprintf(__VA_ARGS__);                              \
		if (r >= MAX_PLATFORM_ID_LEN){                              \
			fprintf(stderr, "String too long");                     \
			goto end;                                               \
		} else if (r < 0){                                          \
			fprintf(stderr, "snprintf() error");                    \
			goto end;                                               \
		}                                                           \
	} while(0);

struct _conf {
	struct common *shm;
	sock_addr_t local;
	char *config_file;
	char *dest_platform_id;
	char *incoming_path;
	char *input_path;
	int not_ordered;
	int timeout;
	long payload_size;
	long bundle_lifetime;
};

/* Hop info */
typedef struct _hop {
	sock_addr_t addr;
	struct timeval report_tv;
	double rtt;
	int showed;
	struct _hop  *next;
} hop;
/**/

/* Route info */
struct _route {
	struct timespec start;
	uint64_t start_timestamp;

	int next_seq;
	hop *hops_list;
	hop *dest;

	int term_lines;
};
/**/

struct _route route = {{0}};
struct _conf conf = {0};

/* time functions */
double diff_time(struct timespec *start, struct timespec *end)
{
	return ((double)(end->tv_sec - start->tv_sec) * 1.0e9 + (double)(end->tv_nsec - start->tv_nsec)) / 10e3;
}

void time_to_str(const struct timeval tv, char *time_str, int time_str_l)
{
	int off = 0;
	struct tm *nowtm;
	nowtm = localtime(&tv.tv_sec);

	off = strftime(time_str, time_str_l, "%Y-%m-%d %H:%M:%S", nowtm);
	sprintf(time_str + off, ".%ld", tv.tv_usec);

}
/**/

int cmp_hops(hop *a, hop *b)
{
	if (a->report_tv.tv_sec < b->report_tv.tv_sec)
		return -1;
	else if (a->report_tv.tv_sec == b->report_tv.tv_sec)
		if (a->report_tv.tv_usec < b->report_tv.tv_usec)
			return -1;
		else if (a->report_tv.tv_usec == b->report_tv.tv_usec)
			return 0;
		else
			return 1;
	else
		return 1;
}

void print_hop(int seq, hop *h)
{
	char strtime[128];

	time_to_str(h->report_tv, strtime, sizeof(strtime));
	printf("%d %-16s\t\tat: %s\t\t(%lf ms)", seq, h->addr.id, strtime, h->rtt);
	if (!h->showed) {
		printf(" *\n");
		h->showed = 1;
	} else {
		printf("\n");
	}

}

void print_dest(hop *d)
{
	char strtime[128];

	time_to_str(d->report_tv, strtime, sizeof(strtime));
	printf("Destination %-16s at: %s\t\t(%lf ms)\n", d->addr.id, strtime, d->rtt);
}

void print_full_route()
{
	int seq = 0;
	hop *tmp = NULL;

	LL_SORT(route.hops_list, cmp_hops);

	// Print ordered route
	LL_FOREACH(route.hops_list, tmp) {
		print_hop(seq, tmp);
		seq++;
		route.term_lines++;
	}
	if (route.dest) {
		print_hop(seq, route.dest);
		route.term_lines++;
	}

	fflush(stdout);

}

void clear_term()
{
	int i = 0, removed_chars = 0;
	for (; i < route.term_lines; i++) {
		removed_chars += fputs("\033[A\033[2K", stdout);
	}
	fseek(stdout, SEEK_CUR, -removed_chars);
	route.term_lines = 0;
}

int ar_extract_cp_timestamp(uint8_t *ar,  uint64_t *timestamp)
{
	int off = 3; // first byte is ar flags, next two status record flags and reason codes
	off += bundle_raw_get_sdnv_off((uint8_t *)ar + off, 2);
	sdnv_decode((uint8_t *)ar + off, (uint64_t *)timestamp);

	return 0;
}

int ar_extract_report_timestamp(uint8_t *ar,  struct timeval *tv)
{
	int off = 3; // first byte is ar flags, next two status record flags and reason codes
	off += sdnv_decode((uint8_t *)ar + off, (uint64_t *)&tv->tv_sec);
	sdnv_decode((uint8_t *)ar + off, (uint64_t *)&tv->tv_usec);

	tv->tv_sec += RFC_DATE_2000;

	return 0;
}

void *recv_forwarded_sr(int *s)
{
	int readed = 0;
	double rtt;
	uint8_t buf[1024] = {0};
	uint64_t cp_timestamp;
	struct timespec rtt_end;
	sock_addr_t recv_addr;


	printf("adtn-traceroute to %s, %d seconds timeout, %ld seconds lifetime, %ld bytes bundles\n", conf.dest_platform_id, conf.timeout, conf.bundle_lifetime, conf.payload_size);

	for (;;) {
		int off = 0;
		struct timeval report_tv;

		readed = adtn_recvfrom(*s, (char *)buf, sizeof(buf), &recv_addr);
		clock_gettime(CLOCK_MONOTONIC_RAW, &rtt_end);

		if (readed <= 0) {
			printf("Something went wrong.\n");
			return (void *)1;
		}

		// Check if we have received an administrative record. We ignore if it is a fragment.
		if ((*buf & AR_SR) != AR_SR)
			continue;

		ar_extract_cp_timestamp(buf, &cp_timestamp);
		if (cp_timestamp != route.start_timestamp)
			continue;

		// Check if it is a reception or a forwarding report
		ar_extract_report_timestamp(buf, &report_tv);
		rtt = diff_time(&route.start, &rtt_end);
		off++; // We now point to the status report body.
		if ((*(buf + off) & SR_CACC) == SR_CACC) {
			hop *new_hop = (hop *)calloc(1, sizeof(hop));
			new_hop->addr = recv_addr;
			new_hop->report_tv = report_tv;
			new_hop->rtt = rtt;
			LL_APPEND(route.hops_list, new_hop);

			if (conf.not_ordered) {
				print_hop(route.next_seq, new_hop);
				route.next_seq++;				
			} else {
				clear_term();
				print_full_route();
			}
		} else if ((*(buf + off) & SR_RECV) == SR_RECV) {
			hop *dest = (hop *)calloc(1, sizeof(hop));
			dest->addr = recv_addr;
			dest->report_tv = report_tv;
			dest->rtt = rtt;

			route.dest = dest;

			if (conf.not_ordered) {
				print_hop(route.next_seq, route.dest);
			} else {
				clear_term();
				print_full_route();				
			}
		}
	}
}

int create_tracerouted_bundle(/*out*/uint8_t **raw_bundle,/*out*/ uint64_t *timestamp_time)
{
	int ret = -1, bundle_raw_l = 0;
	uint64_t flags;
	uint8_t *payload_content = NULL;
	bundle_s *new_bundle;
	payload_block_s *payload;
	char ping_dest[MAX_PLATFORM_ID_LEN], ping_origin[MAX_PLATFORM_ID_LEN];

	// We will create the bundle manually.
	new_bundle = bundle_new();
	*timestamp_time = new_bundle->primary->timestamp_time;

	// Set flags
	flags = H_SR_CACC | H_SR_BREC;
	if (bundle_set_proc_flags(new_bundle, flags) != 0) {
		printf("Error setting processor falgs.");
		goto end;
	}

	// Set destiantion
	SNPRINTF(ping_dest, MAX_PLATFORM_ID_LEN, "%s:%d", conf.dest_platform_id, PING_APP);
	if (bundle_set_destination(new_bundle, ping_dest) != 0) {
		printf("Error setting destination");
		goto end;
	}

	// Set origin. Also at the report-to field
	SNPRINTF(ping_origin, MAX_PLATFORM_ID_LEN, "%s:%d", conf.shm->platform_id, conf.local.adtn_port);
	if (bundle_set_source(new_bundle, ping_origin) != 0) {
		printf("Error setting origin as report-to");
		goto end;
	}
	if (bundle_set_primary_entry(new_bundle, REPORT_SCHEME, ping_origin) != 0) {
		printf("Error setting origin as report-to");
		goto end;
	}

	// Set paylaod
	payload = bundle_new_payload_block();
	payload_content = (uint8_t *)calloc(1, sizeof(uint8_t) * conf.payload_size);
	if (bundle_set_payload(payload, payload_content, conf.payload_size) != 0) {
		printf("Error setting payload");
		goto end;
	}

	if (bundle_add_ext_block(new_bundle, (ext_block_s *)payload)) {
		printf("Error adding payload block to bundle.");
		goto end;
	}

	// Set lifeimte
	new_bundle->primary->lifetime = conf.bundle_lifetime;

	//Create raw bundle
	if ((bundle_raw_l = bundle_create_raw(new_bundle, raw_bundle)) <= 0) {
		printf("Error creating raw bundle");
		goto end;
	}

	ret = bundle_raw_l;
end:
	if (payload)
		free(payload);
	if (payload_content)
		free(payload_content);
	if (new_bundle)
		bundle_free(new_bundle);

	return ret;
}

int send_tracerouted_bundle(const char *dest_id, const int app_port)
{
	char *bundle_name = NULL;
	int ret = 1, bundle_raw_l = 0;

	uint8_t *bundle_raw = NULL;

	bundle_name = generate_bundle_name();
	bundle_raw_l = create_tracerouted_bundle(&bundle_raw, &route.start_timestamp);
	if (bundle_raw_l <= 0) {
		printf("Error creating tracerouted bundle");
		goto end;
	}

	clock_gettime(CLOCK_MONOTONIC_RAW, &route.start);
	if (write_to(conf.input_path, bundle_name, bundle_raw, bundle_raw_l) != 0) {
		printf("Error puting bundle into the queue.\n");
		goto end;
	}

	ret = 0;
end:
	if (bundle_raw)
		free(bundle_raw);
	if (bundle_name)
		free(bundle_name);

	return ret;
}

static void help(const char *program_name)
{
	printf( "%s is part of the SeNDA aDTN platform\n"
	        "Usage: %s [options] [platform_id]\n"
	        "Supported options:\n"
	        "       [-f | --conf_file] config_file\t\tUse {config_file} config file instead of the default found at "DEFAULT_CONF_FILE_PATH"\n"
	        "       [-s | --size] size\t\t\tSet a payload of {size} bytes to the tracerouted bundle\n"
	        "       [-l | --lifetime] lifetime \t\tSet {lifetime} seconds of lifetime to the tracerouted bundle\n"
	        "       [-t | --timeout] timeout \t\tSet timeout as {timeout} seconds.\n"
	        "       [-n | --not-ordered] \t\t\tDo not show ordered route at each forward report received\n"
	        "       [-h | --help]\t\t\t\tShows this help message\n"
	        , program_name, program_name
	      );
}

static void parse_arguments(int argc, char *const argv[])
{
	int opt = -1, option_index = 0;
	static struct option long_options[] = {
		{"conf_file",       required_argument,      0,      'f'},
		{"size",            required_argument,      0,      's'},
		{"lifetime",        required_argument,      0,      'l'},
		{"timeout",         required_argument,      0,      't'},
		{"not-ordered",     no_argument,        	0,  	'o'},
		{"help",            no_argument,            0,      'h'},
		{0, 0, 0, 0}
	};
	while ((opt = getopt_long(argc, argv, "f:s:l:t:nh", long_options, &option_index))) {
		switch (opt) {
		case 'f':
			conf.config_file = strdup(optarg);
			break;
		case 's':
			conf.payload_size = strtol(optarg, NULL, 10);
			break;
		case 'l':
			conf.bundle_lifetime = strtol(optarg, NULL, 10);
			break;			
		case 't':
			conf.timeout = strtol(optarg, NULL, 10);
			break;
		case 'n':
			conf.not_ordered = 1;
			break;
		case 'h':
			help(argv[0]);
			exit(0);			
		case '?': //Unexpected parameter
			help(argv[0]);
			exit(0);
		default:
			break;
		}

		if (opt == -1)
			break;
	}
	if (argc > 1) {
		conf.dest_platform_id = strdup(argv[optind]); // argv[optind] is the next argument after all option chars
	} else {
		help(argv[0]);
		exit(0);
	}

}

int main(int argc,  char *const *argv)
{
	int s = 0, shm_fd = 0, adtn_port = 0, fd = 0, sig = 0;
	char *data_path = NULL;
	struct stat st;
	pthread_t recv_reports_t;

	/* Initialization */
	sigset_t blocked_sigs = {{0}};
	if (sigaddset(&blocked_sigs, SIGINT) != 0)
		LOG_MSG(LOG__ERROR, errno, "sigaddset()");
	if (sigaddset(&blocked_sigs, SIGTERM) != 0)
		LOG_MSG(LOG__ERROR, errno, "sigaddset()");
	if (sigaddset(&blocked_sigs, SIGALRM) != 0)
		LOG_MSG(LOG__ERROR, errno, "sigaddset()");	
	if (sigprocmask(SIG_BLOCK, &blocked_sigs, NULL) == -1)
		LOG_MSG(LOG__ERROR, errno, "sigprocmask()");

	/* Redirect stderr to /dev/null to avoid that unwated error messages appear in terminal*/
	fd = open("/dev/null", O_RDWR);
	dup2(fd, 2);
	/**/

	/******* Configuration *******/
	parse_arguments(argc, argv);

	if (conf.config_file == NULL)
		conf.config_file = DEFAULT_CONF_FILE_PATH;

	if (conf.payload_size == 0) {
		conf.payload_size = DEFAULT_PAYLOAD_SIZE;
	} else if (conf.payload_size >= MAX_BUNDLE_SIZE - 500) {
		printf("Payload too big, setting default value\n");
		conf.payload_size = DEFAULT_PAYLOAD_SIZE;
	}
	if (conf.bundle_lifetime == 0)
		conf.bundle_lifetime = DEFAULT_LIFETIME;
	if (conf.timeout == 0)
		conf.timeout = DEFAULT_TIMEOUT;

	struct conf_list global_conf = {0};
	if (load_config("global", &global_conf, conf.config_file) != 1) {
		printf("Config file %s not found.\n", conf.config_file);
		goto end;
	}
	if ((data_path = get_option_value("data", &global_conf)) == NULL) {
		printf("Config file does not have the data key set.\n");
		goto end;
	}
	if (load_shared_memory_from_path(data_path, &conf.shm, &shm_fd, 0) != 0) {
		printf("Is platform running?\n");
		goto end;
	}

	asprintf(&conf.input_path, "%s/%s", conf.shm->data_path, (char *)INPUT_PATH);

	// We will use a random app port to be able to do multiple pings at the same time from the same platform
	// If folder app exists, use another one.
	srand(time(NULL));
	do {
		adtn_port = rand() % (2 ^ 8) - 1;
		asprintf(&conf.incoming_path, "%s%s%d", conf.shm->data_path, (char *)DEST_PATH, adtn_port);

		if (stat(conf.incoming_path, &st) != 0) {
			if (errno != ENOENT) {
				printf("Connection with queue can not be stablished.\n");
				goto end;
			}
			break;
		} else {
			free(conf.incoming_path);
		}
	} while (1);

	conf.local.id = strdup(conf.dest_platform_id);
	conf.local.adtn_port = adtn_port;

	/* Prepare reception */
	s = adtn_socket(conf.shm->config_file);

	if (adtn_bind(s, &conf.local) != 0) {
		printf("adtn_bind could not be performed.\n");
		exit(1);
	}
	/***/

	send_tracerouted_bundle(conf.dest_platform_id, adtn_port);

	pthread_create(&recv_reports_t, NULL, (void *)recv_forwarded_sr, &s);

	/* Waiting for end signal */
	alarm(conf.timeout);
	for (;;) {
		sig = sigwaitinfo(&blocked_sigs, NULL);

		if (sig == SIGINT || sig == SIGTERM) {
			break;
		}
		if (sig == SIGALRM){
			printf("Timeout expired\n");
			break;
		}
	}
	/**/

	pthread_cancel(recv_reports_t);
	pthread_join(recv_reports_t, NULL);

end:
	free_options_list(&global_conf);

	return 0;
}