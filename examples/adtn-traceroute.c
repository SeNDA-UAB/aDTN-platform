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
	sock_addr_t dest;
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

static void help(const char *program_name)
{
	printf( "%s is part of the SeNDA aDTN platform\n"
	        "Usage: %s [options] [platform_id]\n"
	        "Supported options:\n"
	        "       [-f | --conf_file] config_file\t\tUse {config_file} config file instead of the default found at "DEFAULT_CONF_FILE_PATH".\n"
	        "       [-s | --size] size\t\t\tSet a payload of {size} bytes to the tracerouted bundle.\n"
	        "       [-l | --lifetime] lifetime \t\tSet {lifetime} seconds of lifetime to the tracerouted bundle.\n"
	        "       [-t | --timeout] timeout \t\tWait {timeout} seconds for custody report signals.\n"
	        "       [-n | --not-ordered] \t\t\tDo not show ordered route at each custody report received.\n"
	        "       [-h | --help]\t\t\t\tShows this help message.\n"
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

void clear_route()
{
	int i = 0, removed_chars = 0;
	for (; i < route.term_lines; i++) {
		removed_chars += fputs("\033[A\033[2K", stdout);
	}
	fseek(stdout, SEEK_CUR, -removed_chars);
	route.term_lines = 0;
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

		ar_sr_extract_cp_timestamp(buf, &cp_timestamp);
		if (cp_timestamp != route.start_timestamp)
			continue;

		// Check if it is a reception or a forwarding report
		ar_sr_extract_time_of(buf, &report_tv);
		rtt = diff_time(&route.start, &rtt_end) / 1.0e6;
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
				clear_route();
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
				clear_route();
				print_full_route();				
			}
		}
	}
}

int main(int argc,  char *const *argv)
{
	int s = 0, shm_fd = 0, adtn_port = 0, fd = 0, sig = 0, ping_flags = 0, timestamp_time_l = sizeof(route.start_timestamp);
	char *data_path = NULL, ping_origin[MAX_PLATFORM_ID_LEN] = {0};
	struct stat st;
	pthread_t recv_reports_t;
	uint8_t *payload_content;

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

	conf.local.id = strdup(conf.shm->platform_id);
	conf.local.adtn_port = adtn_port;

	conf.dest.id = conf.dest_platform_id;
	conf.dest.adtn_port = adtn_port;

	/* Prepare reception */
	s = adtn_socket(conf.shm->config_file);

	if (adtn_bind(s, &conf.local) != 0) {
		printf("adtn_bind could not be performed.\n");
		exit(1);
	}
	/***/

	/* Send tracerotued bundle */
	ping_flags = H_DESS | H_NOTF | H_SR_BREC | H_SR_CACC;
	if (adtn_setsockopt(s, OP_PROC_FLAGS, &ping_flags) != 0)
		goto end;
	SNPRINTF(ping_origin, MAX_PLATFORM_ID_LEN, "%s:%d", conf.shm->platform_id, conf.local.adtn_port);
	if (adtn_setsockopt(s, OP_REPORT, ping_origin) != 0)
		goto end;
	if (adtn_setsockopt(s, OP_LIFETIME, &conf.bundle_lifetime) != 0)
		goto end;

	payload_content = (uint8_t *)calloc(1, sizeof(uint8_t) * conf.payload_size);
	if (adtn_sendto(s, payload_content, conf.payload_size, conf.dest) != conf.payload_size) {
		printf("Error sending bundle to tracerotue.\n");
		goto end;
	}
	clock_gettime(CLOCK_MONOTONIC_RAW, &route.start);

	if (adtn_getsockopt(s, OP_LAST_TIMESTAMP, &route.start_timestamp, &timestamp_time_l) != 0 || timestamp_time_l == 0){
		printf("Error getting timestamp of the bundle to traceroute");
		goto end;
	}
	/**/

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