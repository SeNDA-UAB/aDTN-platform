#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/inotify.h>
#include <arpa/inet.h>
#include <linux/limits.h>

#include "common/include/bundle.h"
#include "common/include/paths.h"
#include "common/include/init.h"
#include "common/include/log.h"
#include "common/include/queue.h"
#include "common/include/rit.h"

#define INFO_MSG(...) do {if (DEBUG) info_msg(__VA_ARGS__);} while(0);
#define QUEUE_SOCKNAME "/rec-queue.sock"
#define INOTIFY_EVENT_SIZE  ( sizeof (struct inotify_event) + NAME_MAX + 1)
#define INOTIFY_EVENT_BUF (1*INOTIFY_EVENT_SIZE)
#define IN_SOCK_TIMEOUT 10

typedef struct {
	char *id;
	int app_port;
} endpoint_t;

typedef struct {
	int queue_conn;
	char *spool_path; // Bundles received from and to the network
	char *input_path; // Bundles created locally by the api (are catched by the inotify thread)
	char *destination_path; // Bundles are stored here if its destination is this platform
	char *platform_ip;
	char *platform_id;
	int platform_port;
} global_vars;

global_vars g_vars;


/***************/
static int mkdir_in(const char *in_path, const char *path, mode_t mode, /*out*/ char **full_path_out)
{
	int ret = 0, full_path_l = 0, do_free = 1;
	char *tmp_path = NULL, **full_path = NULL;

	if (full_path_out != NULL) {
		full_path = full_path_out;
		do_free = 0;
	} else {
		full_path = &tmp_path;
	}

	full_path_l = strlen(in_path) + strlen(path) + 1;
	*full_path = (char *)malloc(full_path_l);
	snprintf(*full_path, full_path_l, "%s%s", in_path, path);

	if (mkdir(*full_path, mode) != 0) {
		if (errno != EEXIST) {
			err_msg(true, "Error creating %s", *full_path);
			ret = 1;
			goto end;
		}
	}
	if (chmod(*full_path, mode) != 0) { //avoid umask restriction
		err_msg(true, "Error changing permisions to %s", *full_path);
		ret = 1;
		goto end;
	}

end:
	if (do_free)
		free(*full_path);

	return ret;
}

static int create_paths(const char *data_path)
{
	int ret = 0;
	mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO; //chmod 777                                 //chmod 777
	ret |= mkdir_in(data_path, INPUT_PATH, mode, &g_vars.input_path);
	ret |= mkdir_in(data_path, DEST_PATH, mode, &g_vars.destination_path);
	mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH; //chmod 755
	ret |= mkdir_in(data_path, SPOOL_PATH, mode, &g_vars.spool_path);

	return ret;
}

static int get_file_size(FILE *fd)
{
	int total_bytes;

	if (fd == NULL)
		return 0;
	fseek(fd, 0L, SEEK_END);
	total_bytes = (int)ftell(fd);
	fseek(fd, 0, SEEK_SET);

	return total_bytes;
}

static ssize_t load_bundle(const char *file, uint8_t **raw_bundle)
{
	FILE *fd = NULL;
	ssize_t raw_bundle_l = 0;
	ssize_t ret = -1;

	if (raw_bundle == NULL)
		goto end;

	if ((fd = fopen(file, "r")) <= 0) {
		err_msg(true, "Error loading bundle %s", file);
		goto end;
	}

	if ((raw_bundle_l = get_file_size(fd)) <= 0)
		goto end;

	*raw_bundle = (uint8_t *) malloc(raw_bundle_l);
	if (fread(*raw_bundle, sizeof(**raw_bundle), raw_bundle_l, fd) != raw_bundle_l) {
		err_msg(true, "read()");
		goto end;
	}

	if (fclose(fd) != 0)
		err_msg(true, "close()");

	ret = raw_bundle_l;
end:

	return ret;
}

static int parse_destination(const char *destination, endpoint_t *ep)
{
	int ret = 1;
	const char *p = NULL;

	// Search ip
	if ((p = strchr(destination, ':')) == NULL)
		goto end;
	ep->id = (char *)malloc(p - destination + 1);
	strncpy(ep->id, destination, p - destination);
	ep->id[p - destination] = '\0';

	// Get app port
	ep->app_port = strtol(++p, NULL, 10);

	// Search for app port
	// if ((p = strchr(p, ':')) == NULL)
	//  goto end;
	// ep->app_port = strtol(++p, NULL, 10);

	ret = 0;
end:

	return ret;
}

// TODO: replace utils.h
static char *generate_bundle_name()
{
	struct timeval t_now = {0};
	char *name = NULL;

	if (gettimeofday(&t_now, NULL) < 0)
		goto end;

	asprintf(&name, "%ld-%ld.bundle", t_now.tv_sec, t_now.tv_usec);
end:

	return name;
}

// TODO: replace utils.h
static int write_bundle_to(const char *path, const char *name, const uint8_t *raw_bundle, const ssize_t raw_bundle_l)
{
	FILE *dest = NULL;
	int ret = 1;
	char *full_path = NULL;


	asprintf(&full_path, "%s%s", path, name);

	if (raw_bundle_l == 0) {
		err_msg(false, "Trying to write empty bundle %s");
		goto end;
	}

	if ((dest = fopen(full_path, "w")) == NULL) {
		err_msg(true, "Error opening %s", full_path);
		goto end;
	}

	if (fwrite(raw_bundle, sizeof(*raw_bundle), raw_bundle_l, dest) != raw_bundle_l) {
		err_msg(true, "Error writing to %s", full_path);
		goto end;
	}

	if (fclose(dest) != 0)
		err_msg(true, "Error closing %s", full_path);


	INFO_MSG("Bundle stored into %s", full_path);
	ret = 0;
end:
	if (full_path)
		free(full_path);

	return ret;
}

#define SUBSC_PATH "%s/subscribed"

int get_platform_subscriptions(char **subscribed)
{
	char subs_path[MAX_RIT_PATH];
	int r = 0, ret = 1;

	if (subscribed == NULL)
		goto end;

	r = snprintf(subs_path, sizeof(subs_path), SUBSC_PATH, g_vars.platform_id);

	if (r > sizeof(subs_path)) {
		err_msg(false, "Rit path too long");
		goto end;
	} else if (r < 0) {
		err_msg(true, "snprintf()");
		goto end;
	}

	*subscribed = rit_getValue(subs_path);

	if (*subscribed == NULL || **subscribed == '\0') {
		ret = 1;
		goto end;
	}

	ret = 0;
end:

	return ret;
}

int subscribed_to(const char *dest)
{
	int ret = 0;
	char *subscribed = NULL, *next_subscr = NULL;

	if (get_platform_subscriptions(&subscribed) != 0)
		goto end;

	next_subscr = strtok(subscribed, ",");
	while (next_subscr != NULL) {
		if (strcmp(dest, next_subscr) == 0) {
			ret = 1;
			goto end;
		}
		next_subscr = strtok(NULL, ",");
	}

end:
	if (subscribed)
		free(subscribed);

	return ret;
}

/***************/
int delegate_bundle_to_app(const uint8_t *raw_bundle, const int raw_bundle_l, const endpoint_t ep)
{
	int ret = 1;
	char app_path[7];
	char *bundle_name = NULL, *bundle_dest_path = NULL;
	mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;      //chmod 755

	snprintf(app_path, sizeof(app_path), "%d/", ep.app_port);
	if (mkdir_in(g_vars.destination_path, app_path, mode, &bundle_dest_path) != 0)
		goto end;

	if ((bundle_name = generate_bundle_name()) == NULL)
		goto end;

	if (write_bundle_to(bundle_dest_path, bundle_name, raw_bundle, raw_bundle_l) != 0)
		goto end;

	ret = 0;
end:
	if (bundle_name)
		free(bundle_name);
	if (bundle_dest_path)
		free(bundle_dest_path);

	return ret;
}

int delegate_bundle_to_receiver(const uint8_t *raw_bundle, const int raw_bundle_l)
{
	int ret = 1;
	char *bundle_name = NULL;

	if ((bundle_name = generate_bundle_name()) == NULL)
		goto end;

	if (write_bundle_to(g_vars.input_path, bundle_name, raw_bundle, raw_bundle_l) != 0)
		goto end;

	ret = 0;
end:
	if (bundle_name)
		free(bundle_name);

	return ret;
}

int put_bundle_into_queue(const uint8_t *raw_bundle, const int raw_bundle_l)
{
	int ret = 1;
	char *bundle_name = NULL;

	if ((bundle_name = generate_bundle_name()) == NULL)
		goto end;

	if (write_bundle_to(g_vars.spool_path, bundle_name, raw_bundle, raw_bundle_l) != 0)
		goto end;

	if (queue(bundle_name, g_vars.queue_conn) != 0)
		goto end;

	ret = 0;
end:
	if (bundle_name)
		free(bundle_name);

	return ret;
}

int process_status_report_req_flags(const uint8_t *raw_bundle, const int raw_bundle_l)
{
	int ret = -1;
	uint64_t flags;
	if (bundle_raw_get_proc_flags(raw_bundle, &flags) != 0) {
		err_msg(false, "Error getting proc flags.");
		goto end;
	}

	// If SR_RECV bit is set we have to create a reception notification bundle and put it into the queue
	if ((flags & H_SR_BREC) == H_SR_BREC) {
		char origin[MAX_PLATFORM_ID_LEN];
		char *report_to = NULL;
		uint8_t *bundle_sr_raw = NULL;
		int bundle_sr_raw_l = 0;
		bundle_s *bundle_sr = NULL;
		struct timeval tv;


		INFO_MSG("Received bundle with bit SR_RECV set");
		INFO_MSG("Creating response to bundle with SR_RECV bit");

		if (snprintf(origin, sizeof(origin), "%s:%d", g_vars.platform_id, PING_APP) <= 0) {
			err_msg(true, "snprintf()");
			goto end_SR_RECV;
		}

		if (gettimeofday(&tv, NULL) != 0)
			goto end;

		//Create reply bundle
		if ((bundle_sr = bundle_sr_new_reception_status(origin, tv, raw_bundle, raw_bundle_l)) == NULL) {
			err_msg(false, "Error creating response bundle to the bundle reception reporting request");
			goto end_SR_RECV;
		}
		if ((bundle_sr_raw_l = bundle_create_raw(bundle_sr, &bundle_sr_raw)) <= 0) {
			err_msg(false, "Error creating raw response bundle to the bundle reception reporting request");
			goto end_SR_RECV;
		}

		//Reprocess bundle
		if (delegate_bundle_to_receiver(bundle_sr_raw, bundle_sr_raw_l) != 0) {
			err_msg(false, "Error putting response bundle to the bundle reception reporting request into the queue");
		}

		ret = 2;
end_SR_RECV:
		if (ret == -1)
			err_msg(false, "Response bundle has not been created");
		if (report_to)
			free(report_to);
		if (bundle_sr)
			bundle_free(bundle_sr);
		if (bundle_sr_raw)
			free(bundle_sr_raw);
	} else {
		ret = 0;
	}
end:

	return ret;
}

int process_bundle(const uint8_t *raw_bundle, const int raw_bundle_l)
{
	int ret = 1, err = 0;
	char *full_dest = NULL;
	endpoint_t ep = {0};

	if (bundle_raw_check(raw_bundle, raw_bundle_l) != 0) {
		err_msg(false, "Bundle has errors");
		goto end;
	}

	// It should have a destination (bundle_raw_check checks if it has)
	bundle_get_destination(raw_bundle, (uint8_t **) &full_dest);
	if (parse_destination(full_dest, &ep) != 0) {
		err_msg(false, "Error parsing bundle's destination");
		goto end;
	}


	if (strcmp(ep.id, g_vars.platform_id) == 0) {
		// If we are the destination delegate bundle to application

		INFO_MSG("The bundle is for us. Storing bundle into %s", g_vars.destination_path);

		err = process_status_report_req_flags(raw_bundle, raw_bundle_l);
		if (err < 0) {
			err_msg(false, "Error processing status report request flags");
			goto end;
		}

		if (delegate_bundle_to_app(raw_bundle, raw_bundle_l, ep) != 0) {
			err_msg(false, "Error delegating bundle to the app");
			goto end;
		}

	} else if (subscribed_to(ep.id)) {
		// If the destination is a multicast address delegate bundle to application and put it into the queue

		INFO_MSG("The bundle is destined to a multicast id which we are subscribed");

		if (put_bundle_into_queue(raw_bundle, raw_bundle_l) != 0) {
			err_msg(false, "Error putting bundle into the queue");
			goto end;
		}

		if (delegate_bundle_to_app(raw_bundle, raw_bundle_l, ep) != 0) {
			err_msg(false, "Error delegatin bundle to the app");
			goto end;
		}

	} else {
		// Otherwise put bundle into the queue

		INFO_MSG("The bundle is not for us. Putting bundle into queue");

		if (put_bundle_into_queue(raw_bundle, raw_bundle_l) != 0) {
			err_msg(false, "Error putting bundle into the queue");
			goto end;
		}

	}

	ret = 0;
end:
	if (full_dest)
		free(full_dest);
	if (ep.id)
		free(ep.id);

	return ret;
}

static int process_multiple_bundles(const char *bundles_path)
{
	int ret = 1, process_result = 0;
	char full_path[PATH_MAX] = {0};
	size_t bundles_path_l = 0;
	ssize_t raw_bundle_l = 0;
	uint8_t *raw_bundle = NULL;
	DIR *bp_dfd = NULL;
	struct dirent *ne = NULL;

	if ((bp_dfd = opendir(bundles_path)) == NULL) {
		err_msg(true, "opendir()");
		goto end;
	}

	bundles_path_l = strlen(bundles_path);
	strncpy(full_path, bundles_path, PATH_MAX);
	while ((ne = readdir(bp_dfd)) != NULL) {
		if (ne->d_type != DT_REG)  // Not a regular file
			continue;

		// Create full path
		strncpy(full_path + bundles_path_l, ne->d_name, PATH_MAX - bundles_path_l);

		INFO_MSG("Processing bundle %s", full_path)

		// Load bundle form file
		if ((raw_bundle_l = load_bundle(full_path, &raw_bundle)) == -1) {
			continue;
		}

		// Process bundle
		process_result = process_bundle(raw_bundle, raw_bundle_l);
		if (process_result < 0) {
			err_msg(false, "Error processing %s", full_path);
		} else {
			INFO_MSG("Bundle %s processed correctly", full_path);
			if (unlink(full_path) != 0 && errno != ENOENT)
				err_msg(true, "Error removing bundle %s", full_path);
			else
				INFO_MSG("Bundle %s removed", full_path);
		}

		free(raw_bundle);
	}

	ret = 0;
end:

	if (bp_dfd)
		closedir(bp_dfd);

	return ret;
}


/******* INOTIFY ********/
void *inotify_thread()
{
	int wd = 0, fd_notify = 0, process_result = 0;
	char buffer[INOTIFY_EVENT_BUF], full_path[PATH_MAX];
	char *p = NULL, *new_bundle_name = NULL;
	uint8_t *raw_bundle = NULL;
	ssize_t raw_bundle_l = 0, buffer_l = 0;
	size_t input_path_l = 0;
	struct inotify_event *event = NULL;

	INFO_MSG("Initiated inotify thread");

	if ((fd_notify = inotify_init()) < 0) {
		err_msg(true, "inotify_init()");
		goto end;
	}

	if ((wd = inotify_add_watch(fd_notify, g_vars.input_path, IN_CLOSE_WRITE)) < 0) {
		err_msg(true, "inotify_add_watch()");
		goto end;
	}

	INFO_MSG("Processing stored bundles in %s", g_vars.input_path);
	if (process_multiple_bundles(g_vars.input_path) != 0)
		err_msg(false, "There has been an error processing the stored bundles");
	else
		INFO_MSG("Stored bundles processed correctly");

	INFO_MSG("Waiting for locally created bundles (in %s)", g_vars.input_path);

	input_path_l = strlen(g_vars.input_path);
	memcpy(full_path, g_vars.input_path, input_path_l);
	for (;;) {
		// Wait for a new bundle in the input folder
		buffer_l = read(fd_notify, buffer, INOTIFY_EVENT_BUF);
		if (buffer_l < 0) {
			err_msg(true, "inotify read()");
			continue;
		}

		// For each event
		for (p = buffer; p < buffer + buffer_l; ) {
			event = (struct inotify_event *) p;

			new_bundle_name = event->name;
			strncpy(full_path + input_path_l, new_bundle_name, sizeof(full_path) - input_path_l);

			INFO_MSG("New bundle %s received", full_path);

			// Get contents of the bundle
			if ((raw_bundle_l = load_bundle(full_path, &raw_bundle)) < 0) {
				err_msg(false, "Error loading bundle %s", new_bundle_name);
				goto next_event;
			} else {
				INFO_MSG("Bundle %s loaded", full_path);
			}

			// Process new bundle
			process_result = process_bundle(raw_bundle, raw_bundle_l);
			if (process_result < 0) {
				err_msg(false, "Error processing bunde %s", new_bundle_name);
			} else {
				INFO_MSG("Bundle %s processed correctly", new_bundle_name);
				if (unlink(full_path) != 0 && errno != ENOENT )
					err_msg(true, "Error removing bundle %s", full_path);
				else
					INFO_MSG("Bundle %s removed", full_path);
			}

next_event:
			if (raw_bundle) {
				free(raw_bundle);
				raw_bundle = NULL;
			}

			p += sizeof(struct inotify_event) + event->len;
		}
	}

end:
	close(fd_notify);
	kill(0, SIGINT);
	exit(0);
}

/*******INOTIFY END********/


/******* SOCKET *******/
static int get_and_process_bundle(int *sock)
{
	int ret = 0;
	char *bundle_src_addr = NULL;
	uint16_t bundle_l = 0;
	ssize_t recv_ret = 0;
	uint8_t *bundle = NULL;
	struct sockaddr_in bundle_src = {0};
	socklen_t bundle_src_l = sizeof(bundle_src);
	unsigned recv_l = 0;

	// Get from where we are receiving the bundle
	if (getpeername(*sock, (struct sockaddr *)&bundle_src, &bundle_src_l) != 0) {
		err_msg(true, "getpeername()");
	} else {
		// If we ever change from AF_INET family, we should change this.
		bundle_src_addr = inet_ntoa(bundle_src.sin_addr);
	}
	INFO_MSG("Receiving bundle form %s:%d", bundle_src_addr, ntohs(bundle_src.sin_port));

	// First we get the size of the bundle
	recv_ret = recv(*sock, &bundle_l, sizeof(bundle_l), 0);
	if (recv_ret != sizeof(bundle_l)) {
		if (recv_ret == 0) {
			err_msg(true, "Error receiving bundle length from %s. Probably peer has disconnected.", bundle_src_addr);
		} else if (recv_ret < 0) {
			err_msg(true, "Error receiving bundle length from %s.", bundle_src_addr);
		} else {
			err_msg(true, "Error receiving bundle length from %s. Length not in the correct format", bundle_src_addr);
		}
		ret = 1;
		goto end;
	}


	bundle_l = ntohs(bundle_l);

	// Then we receive the content of the bundle
	bundle = (uint8_t *)malloc(bundle_l * sizeof(uint8_t));
	while (recv_l != bundle_l) {
		recv_ret = recv(*sock, bundle + recv_l, bundle_l - recv_l, 0);
		if (recv_ret == -1) {
			err_msg(true, "Error receiving bundle from %s", bundle_src_addr);
			break;
		} else if (recv_ret == 0) {  // Peer closed the connection
			break;
		}
		recv_l += recv_ret;
	}

	if (recv_l != bundle_l) {
		err_msg(false, "Bundle not received correctly from %s.", bundle_src_addr);
		ret = 1;
		goto end;
	}

	INFO_MSG("Recieved bundle form %s:%d with length %d", bundle_src_addr, ntohs(bundle_src.sin_port), recv_l);


	if (process_bundle(bundle, bundle_l) < 0) {
		err_msg(false, "Error processing bundle from %s", bundle_src_addr);
		ret = 1;
		goto end;
	}

end:
	if (bundle)
		free(bundle);

	close(*sock);
	free(sock);

	pthread_exit(&ret);
}


static void *socket_thread()
{
	int newsockfd, sockfd, ret = 0;
	int *newsockfd_t = NULL;
	struct sockaddr_in serv_addr = {0}, cli_addr = {0};
	socklen_t cli_addr_l = sizeof(cli_addr);
	pthread_t get_and_process_bundle_t = {0};
	struct timeval tv = {.tv_sec = IN_SOCK_TIMEOUT, .tv_usec = 0};
	INFO_MSG("Initiated socket thread");

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		err_msg(true, "socket()");
		ret = 1;
		goto end;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(g_vars.platform_port);
	serv_addr.sin_addr.s_addr = inet_addr(g_vars.platform_ip);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		err_msg(true, "bind()");
		ret = 1;
		goto end;
	}

	if (listen(sockfd, 10) != 0) {
		err_msg(true, "listen()");
		ret = 1;
		goto end;
	}

	INFO_MSG("Waiting for connections");

	for (;;) {
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &cli_addr_l);
		if (newsockfd < 0 && errno == EINTR) {
			break;
		} else if (newsockfd < 0) {
			err_msg(true, "accept()");
			continue;
		}

		// Set timeout to socket
		if (setsockopt(newsockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval)) != 0)
			err_msg(true, "setsockopt()");

		newsockfd_t = (int *)malloc(sizeof(int));
		*newsockfd_t = newsockfd;

		if (pthread_create(&get_and_process_bundle_t, NULL, (void *) get_and_process_bundle, (void *) newsockfd_t) != 0) {
			err_msg(true, "pthread_create()");
			continue;
		}

		if (pthread_detach(get_and_process_bundle_t) != 0){
			err_msg(true, "pthread_detach()");
			continue;
		}
	}

end:
	close(sockfd);
	exit(ret);
}
/***** SOCKET END *****/

int main(int argc, char *argv[])
{
	pthread_t inotify_t, sock_t;
	int sig = 0, ret = EXIT_FAILURE;
	struct common *shm;

	/* Initialization */
	sigset_t blocked_sigs = {{0}};
	if (sigaddset(&blocked_sigs, SIGINT) != 0)
		err_msg(false, "sigaddset()", errno);
	if (sigaddset(&blocked_sigs, SIGTERM) != 0)
		err_msg(false, "sigaddset()", errno);
	if (sigprocmask(SIG_BLOCK, &blocked_sigs, NULL) == -1)
		err_msg(false, "sigprocmask()", errno);

	if (init_adtn_process(argc, argv, &shm) != 0) { //loads shared memory
		err_msg(false, "Error loading shared memory.");
		goto end;
	}

	// Reading from shm
	if (pthread_rwlock_rdlock(&shm->rwlock) != 0) {
		err_msg(true, "pthread_rwlock_rdlock()");
		goto end;
	}

	if (create_paths(shm->data_path) != 0) {
		err_msg(false, "Error creating all the required folders.");
		goto end;
	}

	g_vars.platform_ip = strdup(shm->iface_ip);
	g_vars.platform_id = strdup(shm->platform_id);
	g_vars.platform_port = shm->platform_port;
	g_vars.queue_conn = queue_manager_connect(shm->data_path, QUEUE_SOCKNAME);
	if (g_vars.queue_conn == 0) {
		err_msg(false, "Error connecting to the queue manager.");
		ret = EXIT_FAILURE;
		goto end;
	}

	if (pthread_rwlock_unlock(&shm->rwlock) != 0) {
		err_msg(true, "pthread_rwlock_unlock()");
		goto end;
	}
	// End reading from shm

	/**/

	/* Main threads */
	if (pthread_create(&inotify_t, NULL, inotify_thread, NULL) != 0) {
		err_msg(true, "pthread_create()");
		goto end;
	}
	if (pthread_create(&sock_t, NULL, socket_thread, NULL) != 0) {
		err_msg(true, "pthread_create()");
		goto end;
	}
	/**/

	/* Waiting for end signal */
	for (;;) {
		sig = sigwaitinfo(&blocked_sigs, NULL);

		if (sig == SIGINT || sig == SIGTERM) {
			break;
		}
	}
	/**/

	/* End threads*/
	pthread_cancel(inotify_t);
	pthread_cancel(sock_t);
	/**/

	/* Wait for threads to finish */
	pthread_join(inotify_t, NULL);
	pthread_join(sock_t, NULL);
	/**/


	ret = EXIT_SUCCESS;
end:

	/* Clean*/
	free(g_vars.platform_ip);
	free(g_vars.platform_id);
	queue_manager_disconnect(g_vars.queue_conn, shm->data_path, QUEUE_SOCKNAME);
	exit_adtn_process();
	/**/

	return ret;
}
