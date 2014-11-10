#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "common/include/constants.h"
#include "common/include/config.h"
#include "common/include/bundle.h"
#include "common/include/log.h"
#include "common/include/utils.h"
#include "common/include/queue.h"
#include "common/include/init.h"

#include "api/include/adtn.h"
#include "api/include/uthash.h"

#define DEFAULT_LIFETIME 30
#define DEFAULT_PAYLOAD_SIZE 64
#define DEFAULT_INTERVAL 1000 // Milliseconds

#define PING_CONTENT "Just a ping"
#define QUEUE_SOCKNAME "/ping-queue.sock"
#define SNPRINTF(...)                           \
	do{                                         \
		int r = snprintf(__VA_ARGS__);          \
		if (r >= MAX_PLATFORM_ID_LEN){          \
			err_msg(0, "String too long");      \
			goto end;                           \
		} else if (r < 0){                      \
			err_msg(1, "snprintf()");           \
			goto end;                           \
		}                                       \
	} while(0);

struct _stats {
	int bundles_sent;
	int bundles_recv;
	double rtt_min_time;
	double rtt_max_time;
	double rtt_total_time;
	double total_time;
	int ping_seq;
};

struct _conf {
	struct common *shm;
	sock_addr_t local;
	char *config_file;
	char *dest_platform_id;
	char *incoming_path;
	char *input_path;
	int ping_count;
	long payload_size;
	int ping_interval;
	long ping_lifetime;
};

struct _ping_sent {
	long timestamp;
	int num_seq;
	struct timespec time_sent;
	struct timespec time_recv;
	UT_hash_handle hh;
};

/* Glboals*/
struct _conf conf = {0};
struct _stats stats = {0};
struct _ping_sent *pings_sent = NULL;
int ping_seq = 0;
/**/

static void help(char *program_name)
{
	printf( "%s is part of the SeNDA aDTN platform\n"
	        "Usage: %s [options] [platform_id]\n"
	        "Supported options:\n"
	        "       [-f | --conf_file] config_file\t\tUse {config_file} config file instead of the default found at "DEFAULT_CONF_FILE_PATH"\n"
	        "       [-c | --count] count\t\t\tStop after sending {count} adtn pings\n"
	        "       [-s | --size] size\t\t\tSet a payload of {size} bytes\n"
	        "       [-i | --interval] interval \t\tWait {interval} milliseconds between adtn pings\n"
	        "       [-l | --lifetime] lifetime\t\tSet adtn pings {lifetime} seconds of lifetime\n"
	        "       [-h | --help]\t\t\t\tShows this help message\n"
	        , program_name, program_name);
}

static void parse_arguments(int argc, char *const argv[])
{
	int opt = -1, option_index = 0;
	static struct option long_options[] = {
		{"help",            no_argument,            0,      'h'},
		{"conf_file",       no_argument,            0,      'f'},
		{"count",           no_argument,            0,      'c'},
		{"size",            no_argument,            0,      's'},
		{"interval",        no_argument,            0,      'i'},
		{"lifetime",        no_argument,            0,      'l'},
		{0, 0, 0, 0}
	};
	while ((opt = getopt_long(argc, argv, "hf:c:s:i:l:", long_options, &option_index))) {
		switch (opt) {
		case 'h':
			help(argv[0]);
			exit(0);
		case 'f':
			conf.config_file = strdup(optarg);
			break;
		case 'c':
			conf.ping_count = strtol(optarg, NULL, 10);
			break;
		case 's':
			conf.payload_size = strtol(optarg, NULL, 10);
			break;
		case 'i':
			conf.ping_interval = strtol(optarg, NULL, 10);
			break;
		case 'l':
			conf.ping_lifetime = strtol(optarg, NULL, 10);
			break;
		case '?':           //Unexpected parameter
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

double diff_time(struct timespec *start, struct timespec *end)
{
	return (double)(end->tv_sec - start->tv_sec) * 1.0e9 + (double)(end->tv_nsec - start->tv_nsec);
}

void time_to_str(const struct timeval tv, char *time_str, int time_str_l)
{
	struct tm *nowtm;
	nowtm = localtime(&tv.tv_sec);

	strftime(time_str, time_str_l, "%Y-%m-%d %H:%M:%S", nowtm);
}

int create_ping(uint8_t **raw_bundle, int payload_size, uint64_t *timestamp_time)
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
	flags = H_SR_BREC;
	if (bundle_set_proc_flags(new_bundle, flags) != 0) {
		printf("Error stting processor falgs.");
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
		printf("Error addind payload block to bundle.");
		goto end;
	}

	// Set lifeimte
	new_bundle->primary->lifetime = conf.ping_lifetime;

	//Create raw bundle
	if ((bundle_raw_l = bundle_create_raw(new_bundle, raw_bundle)) <= 0) {
		printf("Error creating raw bundle");
		goto end;
	}

	// Check for errors
	if (bundle_raw_check(*raw_bundle, bundle_raw_l) != 0) {
		printf("BUNDLE WITH EROORS\n");
		pause();
	}

	ret = bundle_raw_l;
end:
	if (payload_content)
		free(payload_content);
	if (new_bundle)
		bundle_free(new_bundle);

	return ret;
}

void show_stats()
{
	int perc_lost = 0;
	double avg = 0;

	if (stats.bundles_recv == 0)
		avg = 0;
	else
		avg = stats.rtt_total_time / stats.bundles_recv;

	if (stats.bundles_sent == 0)
		perc_lost = 0;
	else
		perc_lost = ( 1.0 -  ((double) stats.bundles_recv / (double) stats.bundles_sent)) * 100;


	printf(
	    "\n"
	    "-- %s ping statistics --\n"
	    "%d bundles transmitted, %d bundles received, %d%% packet loss, time %lfms\n"
	    "rtt min/avg/max = %lf/%lf/%lf ms\n"
	    , conf.dest_platform_id,
	    stats.bundles_sent, stats.bundles_recv, perc_lost, stats.total_time,
	    stats.rtt_min_time, avg, stats.rtt_max_time
	);

}

int sr_exctract_ping_timestamp(uint8_t *sr,  uint64_t *timestamp)
{
	int off = 2;
	off += bundle_raw_get_sdnv_off((uint8_t *)sr + off, 2);
	sdnv_decode((uint8_t *)sr + off, (uint64_t *)timestamp);

	return 0;
}

int sr_extract_pong_reception_time(uint8_t *sr,  uint64_t *reception_time)
{
	sdnv_decode((uint8_t *)sr + 2, (uint64_t *)reception_time);
	*reception_time += RFC_DATE_2000;

	return 0;
}

void *recv_pings(int *s)
{
	char buf[1024] = {0}, strtime[128];
	int readed = 0;
	double rtt = 0;
	uint64_t ts_time = 0;
	sock_addr_t recv_addr;
	struct timeval tv = {0};

	// Wait pong
	for (;;) {
		struct _ping_sent *recv_ping = NULL;

		readed = adtn_recvfrom(*s, buf, sizeof(buf), &recv_addr);
		if (readed <= 0) {
			printf("Something went wrong.\n");
			return (void *)1;
		}

		sr_exctract_ping_timestamp((uint8_t *)buf, &ts_time);

		/* Retrieve ping info */
		HASH_FIND(hh, pings_sent, &ts_time, sizeof(long), recv_ping);
		if (recv_ping != NULL) {
			clock_gettime(CLOCK_MONOTONIC_RAW, &recv_ping->time_recv);

			/* Update stats*/
			stats.bundles_recv++;

			rtt = diff_time( &recv_ping->time_sent, &recv_ping->time_recv) / 1.0e6;
			if (rtt < stats.rtt_min_time || stats.rtt_min_time == 0)
				stats.rtt_min_time = rtt;
			if (rtt > stats.rtt_max_time || stats.rtt_max_time == 0)
				stats.rtt_max_time = rtt;
			stats.rtt_total_time += rtt;
			/**/

			/* Extract reception time */
			sr_extract_pong_reception_time((uint8_t *)buf, (uint64_t *)&tv.tv_sec);
			time_to_str(tv, strtime, sizeof(strtime));
			/**/

			printf("%s received ping at %s: seq=%d time=%lfms\n", conf.dest_platform_id, strtime, recv_ping->num_seq, rtt);

			/* Remove ping from hash table */
			HASH_DELETE(hh, pings_sent, recv_ping);
			free(recv_ping);
			/**/

			if (stats.bundles_recv == conf.ping_count) {
				kill(getpid(), SIGINT);
			}
		}
	}
}

void *send_pings(int *s)
{
	char *bundle_name = NULL;
	int bundle_raw_l = 0;

	uint8_t *bundle_raw = NULL;
	uint64_t timestamp_time = 0;

	struct _ping_sent *new_ping = NULL;

	printf("PING %s %ld bytes of payload. %ld seconds of lifetime\n", conf.dest_platform_id, conf.payload_size, conf.ping_lifetime);

	for (;;) {

		/* Prepare ping */
		bundle_name = generate_bundle_name();
		bundle_raw_l = create_ping(&bundle_raw, conf.payload_size, &timestamp_time);
		/**/

		/* Store ping info */
		new_ping = (struct _ping_sent *)malloc(sizeof(struct _ping_sent));
		new_ping->timestamp = timestamp_time;
		new_ping->num_seq = ++ping_seq;
		clock_gettime(CLOCK_MONOTONIC_RAW, &new_ping->time_sent);
		HASH_ADD(hh, pings_sent, timestamp, sizeof(long), new_ping);
		/**/

		if (write_to(conf.input_path, bundle_name, bundle_raw, bundle_raw_l) != 0) {
			printf("Error puting bundle into the queue.\n");
		}

		/* Update stats*/
		stats.bundles_sent++;
		stats.ping_seq++;
		/**/

		if (bundle_name)
			free(bundle_name);
		if (bundle_raw)
			free(bundle_raw);

		if (stats.ping_seq == conf.ping_count) {
			sleep(conf.ping_lifetime);
			kill(getpid(), SIGINT);
		}

		usleep(conf.ping_interval * 1000);
	}
}

/*
    argv[argc-1]: Bundle destiantion ID
 */
int main(int argc,  char *const *argv)
{
	int ret = 1, s = 0, shm_fd = 0, adtn_port = 0, fd = 0, sig = 0;
	char *bundle_name = NULL, *data_path = NULL;
	struct stat st;
	struct timespec total_start = {0}, total_end = {0};
	pthread_t send_pings_t, recv_pings_t;

	/* Initialization */
	sigset_t blocked_sigs = {{0}};
	if (sigaddset(&blocked_sigs, SIGINT) != 0)
		err_msg(false, "sigaddset()", errno);
	if (sigaddset(&blocked_sigs, SIGTERM) != 0)
		err_msg(false, "sigaddset()", errno);
	if (sigprocmask(SIG_BLOCK, &blocked_sigs, NULL) == -1)
		err_msg(false, "sigprocmask()", errno);

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

	if (conf.ping_interval == 0)
		conf.ping_interval = DEFAULT_INTERVAL;

	if (conf.ping_lifetime == 0)
		conf.ping_lifetime = DEFAULT_LIFETIME;


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
		adtn_port = rand() % 65536;
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

	/* Start ping!*/
	clock_gettime(CLOCK_MONOTONIC_RAW, &total_start);

	pthread_create(&recv_pings_t, NULL, (void *)recv_pings, &s);
	pthread_create(&send_pings_t, NULL, (void *)send_pings, &s);

	/* Waiting for end signal */
	for (;;) {
		sig = sigwaitinfo(&blocked_sigs, NULL);

		if (sig == SIGINT || sig == SIGTERM) {
			break;
		}
	}
	/**/

	pthread_cancel(recv_pings_t);
	pthread_cancel(send_pings_t);

	pthread_join(recv_pings_t, NULL);
	pthread_join(send_pings_t, NULL);

	clock_gettime(CLOCK_MONOTONIC_RAW, &total_end);
	stats.total_time = diff_time( &total_start, &total_end) / 1.0e6;

	/* End ping! */

	// Show stats
	show_stats();

	ret = 0;
end:
	if (s)
		adtn_shutdown(s);
	if (bundle_name)
		free(bundle_name);
	if (conf.incoming_path)
		free(conf.incoming_path);
	if (fd != 2)
		close(fd);
	unmap_shared_memory(conf.shm);

	return ret;
}
