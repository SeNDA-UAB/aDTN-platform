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

#include "bundleAgent/common/include/constants.h"
#include "bundleAgent/common/include/config.h"
#include "bundleAgent/common/include/bundle.h"
#include "bundleAgent/common/include/log.h"
#include "bundleAgent/common/include/utils.h"
#include "bundleAgent/common/include/queue.h"
#include "bundleAgent/common/include/init.h"

#include "lib/api/include/adtn.h"
#include "lib/api/include/uthash.h"

#define DEFAULT_LIFETIME 30
#define DEFAULT_PAYLOAD_SIZE 64
#define DEFAULT_INTERVAL 1000 // Milliseconds

#define PING_CONTENT "Just a ping"
#define QUEUE_SOCKNAME "/ping-queue.sock"
#define SNPRINTF(...)                                               \
	do{                                                             \
		int r = snprintf(__VA_ARGS__);                              \
		if (r >= MAX_PLATFORM_ID_LEN){                              \
			LOG_MSG(LOG__ERROR, false, "String too long");          \
			goto end;                                               \
		} else if (r < 0){                                          \
			LOG_MSG(LOG__ERROR, true, "snprintf()");                \
			goto end;                                               \
		}                                                           \
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
	sock_addr_t dest;
	char *config_file;
	char *dest_platform_id;
	char *incoming_path;
	char *input_path;
	int ping_count;
	long payload_size;
	int ping_interval;
	long ping_lifetime;
};

typedef struct _ping {
	long timestamp;
	int num_seq;
	struct timespec time_sent;
	struct timespec time_recv;
	UT_hash_handle hh;
} ping;

/* Glboals*/
struct _conf conf = {0};
struct _stats stats = {0};
ping *pings_sent = NULL;

pthread_mutex_t ping_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ping_cond = PTHREAD_COND_INITIALIZER;
int last_ping_added = -1;
/**/

static void help(char *program_name)
{
	printf( "%s is part of the SeNDA aDTN platform\n"
	        "Usage: %s [options] [platform_id]\n"
	        "Supported options:\n"
	        "       [-f | --conf_file] config_file\t\tUse {config_file} config file instead of the default found at "DEFAULT_CONF_FILE_PATH"\n"
	        "       [-s | --size] size\t\t\tSet a payload of {size} bytes to ping bundles.\n"
	        "       [-l | --lifetime] lifetime\t\tSet {lifetime} seconds of lifetime to the ping bundles.\n"
	        "       [-c | --count] count\t\t\tStop after sending {count} ping bundles.\n"
	        "       [-i | --interval] interval \t\tWait {interval} milliseconds between pings.\n"
	        "       [-h | --help]\t\t\t\tShows this help message.\n"
	        , program_name, program_name);
}

static void parse_arguments(int argc, char *const argv[])
{
	int opt = -1, option_index = 0;
	static struct option long_options[] = {
		{"conf_file",       required_argument,          0,      'f'},
		{"size",            required_argument,          0,      's'},
		{"lifetime",        required_argument,          0,      'l'},
		{"count",           required_argument,          0,      'c'},
		{"interval",        required_argument,          0,      'i'},
		{"help",            no_argument,                0,      'h'},		
		{0, 0, 0, 0}
	};
	while ((opt = getopt_long(argc, argv, "f:s:l:c:i:h", long_options, &option_index))) {
		switch (opt) {
		case 'f':
			conf.config_file = strdup(optarg);
			break;
		case 's':
			conf.payload_size = strtol(optarg, NULL, 10);
			break;
		case 'l':
			conf.ping_lifetime = strtol(optarg, NULL, 10);
			break;		
		case 'c':
			conf.ping_count = strtol(optarg, NULL, 10);
			break;
		case 'i':
			conf.ping_interval = strtol(optarg, NULL, 10);
			break;
		case 'h':
			help(argv[0]);
			exit(0);
		case '?':           //Unexpected parameter
			help(argv[0]);
			exit(0);
		default:
			break;
		}

		if (opt == -1)
			break;
	}
	if (argc > 1 && optind < argc) {
		conf.dest_platform_id = strdup(argv[optind]); // argv[optind] is the next argument after all option chars
	} else {
		help(argv[0]);
		exit(0);
	}

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

void recv_pings(int *s)
{
	char buf[1024] = {0}, strtime[128];
	int readed = 0;
	double rtt = 0;
	uint64_t ts_time = 0;
	sock_addr_t recv_addr;
	struct timeval tv = {0};

	// Wait pong
	for (;;) {
		ping *recv_ping = NULL;

		readed = adtn_recvfrom(*s, buf, sizeof(buf), &recv_addr);
		if (readed <= 0) {
			printf("Something went wrong.\n");
			goto end;
		}
		while (last_ping_added != stats.ping_seq)
			pthread_cond_wait(&ping_cond, &ping_mutex);

		ar_sr_extract_cp_timestamp((uint8_t *)buf, &ts_time);

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
			ar_sr_extract_time_of((uint8_t *)buf, &tv);
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
		pthread_mutex_unlock(&ping_mutex);
	}

end:
	kill(getpid(), SIGINT);	
}

void send_pings(int *s)
{
	char ping_origin[MAX_PLATFORM_ID_LEN];

	uint8_t *payload_content = NULL;
	uint32_t ping_flags;
	uint64_t timestamp_time = 0;
	int timestamp_time_l = sizeof(timestamp_time);

	ping *new_ping = NULL;

	printf("PING %s %ld bytes of payload. %ld seconds of lifetime\n", conf.dest_platform_id, conf.payload_size, conf.ping_lifetime);

	// Configure socket
	ping_flags = H_DESS | H_NOTF | H_SR_BREC;
	if (adtn_setsockopt(*s, OP_PROC_FLAGS, &ping_flags) != 0)
		goto end;
	SNPRINTF(ping_origin, MAX_PLATFORM_ID_LEN, "%s:%d", conf.shm->platform_id, conf.local.adtn_port);
	if (adtn_setsockopt(*s, OP_REPORT, ping_origin) != 0)
		goto end;
	if (adtn_setsockopt(*s, OP_LIFETIME, &conf.ping_lifetime) != 0)
		goto end;

	payload_content = (uint8_t *)calloc(1, sizeof(uint8_t) * conf.payload_size);

	for (;;) {
		pthread_mutex_lock(&ping_mutex);
		if (adtn_sendto(*s, payload_content, conf.payload_size, conf.dest) != conf.payload_size) {
			if(errno == EMSGSIZE) {
				printf("Error sending ping, Payload too big.\n");
				goto end;
			}
			else {
				printf("Error sending ping.\n");
			}
		} else {
			/* Store ping info */
			if (adtn_getsockopt(*s, OP_LAST_TIMESTAMP, &timestamp_time, &timestamp_time_l) != 0 || timestamp_time_l == 0)
				continue;

			new_ping = (ping *)malloc(sizeof(ping));
			new_ping->timestamp = timestamp_time;
			new_ping->num_seq = ++stats.ping_seq;
			clock_gettime(CLOCK_MONOTONIC_RAW, &new_ping->time_sent);
			HASH_ADD(hh, pings_sent, timestamp, sizeof(long), new_ping);
			/**/

			stats.bundles_sent++;
		}
		last_ping_added = stats.ping_seq;
		pthread_cond_broadcast(&ping_cond);
		pthread_mutex_unlock(&ping_mutex);

		if (stats.ping_seq == conf.ping_count) {
			sleep(conf.ping_lifetime);
			kill(getpid(), SIGINT);
		}

		usleep(conf.ping_interval * 1000);
	}

end:
	kill(getpid(), SIGINT);
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
		LOG_MSG(LOG__ERROR, errno, "sigaddset()");
	if (sigaddset(&blocked_sigs, SIGTERM) != 0)
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

	if (conf.payload_size == 0)
		conf.payload_size = DEFAULT_PAYLOAD_SIZE;

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
