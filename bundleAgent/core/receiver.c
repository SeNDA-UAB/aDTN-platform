/*
* Copyright (c) 2014 SeNDA
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
*/

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

#include "common/include/common.h"
#include "common/include/init.h"
#include "common/include/queue.h"
#include "common/include/utils.h"

#define QUEUE_SOCKNAME "/rec-queue.sock"
#define INOTIFY_EVENT_SIZE  ( sizeof (struct inotify_event) + NAME_MAX + 1)
#define INOTIFY_EVENT_BUF (1*INOTIFY_EVENT_SIZE)

typedef struct {
	char *id;
	int app_port;
} endpoint_s;

typedef struct {
	int queue_conn;
	char *queue_path; // Bundles received from and to the network
	char *input_path; // Bundles created locally by the api (are catched by the inotify thread)
	char *destination_path; // Bundles are stored here if its destination is this platform
	char *platform_ip;
	char *platform_id;
	int platform_port;
} global_vars;

global_vars g_vars;


/******* UTILS ********/
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

	full_path_l = strlen(in_path) + strlen(path) + 2;
	*full_path = (char *)malloc(full_path_l);
	snprintf(*full_path, full_path_l, "%s/%s", in_path, path);

	if (mkdir(*full_path, mode) != 0) {
		if (errno != EEXIST) {
			LOG_MSG(LOG__ERROR, true, "Error creating %s", *full_path);
			ret = 1;
			goto end;
		}
	}
	if (chmod(*full_path, mode) != 0) { //avoid umask restriction
		LOG_MSG(LOG__ERROR, true, "Error changing permisions to %s", *full_path);
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
	ret |= mkdir_in(data_path, QUEUE_PATH, mode, &g_vars.queue_path);

	return ret;
}

static ssize_t load_bundle(const char *file, uint8_t **raw_bundle)
{
	FILE *fd = NULL;
	ssize_t raw_bundle_l = 0;
	ssize_t ret = -1;

	if (raw_bundle == NULL)
		goto end;

	if ((fd = fopen(file, "r")) <= 0) {
		LOG_MSG(LOG__ERROR, true, "Error loading bundle %s", file);
		goto end;
	}

	if ((raw_bundle_l = get_file_size(fd)) <= 0)
		goto end;

	*raw_bundle = (uint8_t *) malloc(raw_bundle_l);
	if (fread(*raw_bundle, sizeof(**raw_bundle), raw_bundle_l, fd) != raw_bundle_l) {
		LOG_MSG(LOG__ERROR, true, "read()");
		goto end;
	}

	if (fclose(fd) != 0)
		LOG_MSG(LOG__ERROR, true, "close()");

	ret = raw_bundle_l;
end:

	return ret;
}

static int parse_destination(const char *destination, endpoint_s *ep)
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

	ret = 0;
end:

	return ret;
}

int get_platform_subscriptions(char **subscribed)
{
	char subs_path[MAX_RIT_PATH];
	int r = 0, ret = 1;

	if (subscribed == NULL)
		goto end;

	r = snprintf(subs_path, sizeof(subs_path), SUBSC_PATH, g_vars.platform_id);

	if (r > sizeof(subs_path)) {
		LOG_MSG(LOG__ERROR, false, "Rit path too long");
		goto end;
	} else if (r < 0) {
		LOG_MSG(LOG__ERROR, true, "snprintf()");
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

/******* DELEGATE BUNDLE ********/
int delegate_bundle_to_app(const char *bundle_name, const uint8_t *raw_bundle, const int raw_bundle_l, const endpoint_s ep)
{
	int ret = 1;
	char app_path[7];
	char *bundle_dest_path = NULL;
	mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;      //chmod 755

	snprintf(app_path, sizeof(app_path), "%d/", ep.app_port);
	if (mkdir_in(g_vars.destination_path, app_path, mode, &bundle_dest_path) != 0)
		goto end;


	if (write_to(bundle_dest_path, bundle_name, raw_bundle, raw_bundle_l) != 0)
		goto end;
	LOG_MSG(LOG__INFO, false, "Bundle stored into %s/%s", bundle_dest_path, bundle_name);

	ret = 0;
end:
	if (bundle_dest_path)
		free(bundle_dest_path);

	return ret;
}

int delegate_bundle_to_receiver(const char *bundle_name, const uint8_t *raw_bundle, const int raw_bundle_l)
{
	int ret = 1;

	if (write_to(g_vars.input_path, bundle_name, raw_bundle, raw_bundle_l) != 0)
		goto end;
	LOG_MSG(LOG__INFO, false, "Bundle stored into %s/%s", g_vars.input_path, bundle_name);

	ret = 0;
end:
	return ret;
}

int delegate_bundle_to_queue_manager(const char *bundle_name, const uint8_t *raw_bundle, const int raw_bundle_l)
{
	int ret = 1;

	if (write_to(g_vars.queue_path, bundle_name, raw_bundle, raw_bundle_l) != 0)
		goto end;
	LOG_MSG(LOG__INFO, false, "Bundle stored into %s/%s", g_vars.queue_path, bundle_name);

	if (queue(bundle_name, g_vars.queue_conn) != 0)
		goto end;

	ret = 0;
end:
	return ret;
}
/***************/

/******* ACTIONS ********/
int send_status_report(const sr_status_flags_e status_flag, const char *bundle_name, const uint8_t *raw_bundle, const int raw_bundle_l)
{
	char origin[MAX_PLATFORM_ID_LEN];
	char *report_to = NULL;
	uint8_t *bundle_sr_raw = NULL;
	int bundle_sr_raw_l = 0, ret = 1;
	bundle_s *bundle_sr = NULL;
	struct timeval tv;

	LOG_MSG(LOG__INFO, false, "Creating response to bundle with SR_RECV bit");

	if (snprintf(origin, sizeof(origin), "%s:%d", g_vars.platform_id, PING_APP) <= 0) {
		LOG_MSG(LOG__ERROR, true, "snprintf()");
		goto end;
	}

	if (gettimeofday(&tv, NULL) != 0)
		goto end;

	//Create reply bundle
	if ((bundle_sr = bundle_new_sr(status_flag, 0, origin, tv, raw_bundle)) == NULL) {
		LOG_MSG(LOG__ERROR, false, "Error creating response bundle to the bundle reception reporting request");
		goto end;
	}
	if ((bundle_sr_raw_l = bundle_create_raw(bundle_sr, &bundle_sr_raw)) <= 0) {
		LOG_MSG(LOG__ERROR, false, "Error creating raw response bundle to the bundle reception reporting request");
		goto end;
	}

	//Reprocess bundle
	if (delegate_bundle_to_receiver(bundle_name, bundle_sr_raw, bundle_sr_raw_l) != 0) {
		LOG_MSG(LOG__ERROR, false, "Error putting response bundle to the bundle reception reporting request into the queue");
		goto end;
	}

	ret = 0;
end:
	if (ret == 1)
		LOG_MSG(LOG__ERROR, false, "Response bundle has not been created");
	if (report_to)
		free(report_to);
	if (bundle_sr)
		bundle_free(bundle_sr);
	if (bundle_sr_raw)
		free(bundle_sr_raw);

	return ret;
}

int process_custody_report_request_flag(const char *bundle_name, const uint8_t *raw_bundle, const int raw_bundle_l)
{
	int ret = 1;
	uint64_t flags;
	if (bundle_raw_get_proc_flags(raw_bundle, &flags) != 0) {
		LOG_MSG(LOG__ERROR, false, "Error getting proc flags.");
		goto end;
	}

	// If SR_RECV bit is set we have to send a reception notification
	if ((flags & H_SR_CACC) == H_SR_CACC) {
		LOG_MSG(LOG__INFO, false, "Received bundle with bit H_SR_CACC set");
		if (send_status_report(SR_CACC, bundle_name, raw_bundle, raw_bundle_l) != 0)
			goto end;
	}

	ret = 0;
end:
	return ret;
}


int process_reception_report_request_flag(const char *bundle_name, const uint8_t *raw_bundle, const int raw_bundle_l)
{
	int ret = 1;
	uint64_t flags;
	if (bundle_raw_get_proc_flags(raw_bundle, &flags) != 0) {
		LOG_MSG(LOG__ERROR, false, "Error getting proc flags.");
		goto end;
	}

	// If SR_RECV bit is set we have to send a reception notification
	if ((flags & H_SR_BREC) == H_SR_BREC) {
		LOG_MSG(LOG__INFO, false, "Received bundle with bit H_SR_RECV set");
		if (send_status_report(SR_RECV, bundle_name, raw_bundle, raw_bundle_l) != 0)
			goto end;
	}

	ret = 0;
end:
	return ret;
}
/***************/

/******* BUNDLE PROCESSING ********/
int process_bundle(const char *origin, const uint8_t *raw_bundle, const int raw_bundle_l)
{
	int ret = 1;
	char *full_dest = NULL, *bundle_name = NULL;
	endpoint_s ep = {0};

	if (bundle_raw_check(raw_bundle, raw_bundle_l) != 0) {
		LOG_MSG(LOG__ERROR, false, "Bundle has errors");
		goto end;
	}

	// It should have a destination (bundle_raw_check checks if it has)
	bundle_get_destination(raw_bundle, (uint8_t **) &full_dest);
	if (parse_destination(full_dest, &ep) != 0) {
		LOG_MSG(LOG__ERROR, false, "Error parsing bundle's destination");
		goto end;
	}

	// Generate name of the bundle
	bundle_name = generate_bundle_name(origin);

	if (strcmp(ep.id, g_vars.platform_id) == 0) {
		// If we are the destination delegate bundle to application

		LOG_MSG(LOG__INFO, false, "The bundle is for us. Storing bundle into %s", g_vars.destination_path);

		if (process_reception_report_request_flag(bundle_name, raw_bundle, raw_bundle_l) != 0) {
			LOG_MSG(LOG__ERROR, false, "Error processing status report request flags");
			goto end;
		}

		if (delegate_bundle_to_app(bundle_name, raw_bundle, raw_bundle_l, ep) != 0) {
			LOG_MSG(LOG__ERROR, false, "Error delegating bundle to the app");
			goto end;
		}

	} else if (subscribed_to(ep.id)) {
		// If the destination is a multicast address delegate bundle to application and put it into the queue

		LOG_MSG(LOG__INFO, false, "The bundle is destined to a multicast id which we are subscribed");

		if (delegate_bundle_to_queue_manager(bundle_name, raw_bundle, raw_bundle_l) != 0) {
			LOG_MSG(LOG__ERROR, false, "Error putting bundle into the queue");
			goto end;
		}

		if (delegate_bundle_to_app(bundle_name, raw_bundle, raw_bundle_l, ep) != 0) {
			LOG_MSG(LOG__ERROR, false, "Error delegatin bundle to the app");
			goto end;
		}

	} else {
		// Otherwise put bundle into the queue

		LOG_MSG(LOG__INFO, false, "The bundle is not for us. Putting bundle into queue");

		if (process_custody_report_request_flag(bundle_name, raw_bundle, raw_bundle_l) != 0) {
			LOG_MSG(LOG__ERROR, false, "Error processing status report request flags");
			goto end;
		}

		if (delegate_bundle_to_queue_manager(bundle_name, raw_bundle, raw_bundle_l) != 0) {
			LOG_MSG(LOG__ERROR, false, "Error putting bundle into the queue");
			goto end;
		}

	}

	ret = 0;
end:
	if (bundle_name)
		free(bundle_name);
	if (full_dest)
		free(full_dest);
	if (ep.id)
		free(ep.id);

	return ret;
}

int load_and_process_bundle(const char *bundle_path)
{
	int ret = 1, process_result = 0;
	ssize_t raw_bundle_l = 0;
	uint8_t *raw_bundle = NULL;
	b_name_s bundle_name_s = {{0}};

	LOG_MSG(LOG__INFO, false, "Processing bundle %s", bundle_path);

	// Load bundle form file
	if ((raw_bundle_l = load_bundle(bundle_path, &raw_bundle)) == -1) {
		LOG_MSG(LOG__ERROR, false, "Error loading bundle %s", bundle_path);
		goto end;
	} else {
		LOG_MSG(LOG__INFO, false, "Bundle %s loaded", bundle_path);
	}

	if (parse_bundle_name(bundle_path, &bundle_name_s) != 0) {
		LOG_MSG(LOG__INFO, false, "Error getting bundle %s", bundle_path);
		goto end;
	}

	// Process bundle
	process_result = process_bundle(bundle_name_s.origin, raw_bundle, raw_bundle_l);
	if (process_result < 0) {
		LOG_MSG(LOG__ERROR, false, "Error processing %s", bundle_path);
	} else {
		LOG_MSG(LOG__INFO, false, "Bundle %s processed correctly", bundle_path);
		if (unlink(bundle_path) != 0 && errno != ENOENT)
			LOG_MSG(LOG__ERROR, true, "Error removing bundle %s", bundle_path);
		else
			LOG_MSG(LOG__INFO, false, "Bundle %s removed", bundle_path);
	}

	free(raw_bundle);

	ret = 0;
end:
	return ret;
}


static int process_multiple_bundles(const char *bundles_path)
{
	int ret = 1;
	char full_path[PATH_MAX] = {0};
	size_t bundles_path_l = 0;
	DIR *bp_dfd = NULL;
	struct dirent *ne = NULL;

	if ((bp_dfd = opendir(bundles_path)) == NULL) {
		LOG_MSG(LOG__ERROR, true, "opendir()");
		goto end;
	}

	bundles_path_l = strlen(bundles_path);
	strncpy(full_path, bundles_path, PATH_MAX);
	while ((ne = readdir(bp_dfd)) != NULL) {
		if (ne->d_type != DT_REG)  // Not a regular file
			continue;

		// Create full path
		snprintf(full_path + bundles_path_l, bundles_path_l, "/%s", ne->d_name);

		// Load and process bundle
		load_and_process_bundle(full_path);
	}

	ret = 0;
end:
	if (bp_dfd)
		closedir(bp_dfd);

	return ret;
}
/***************/


/******* LOCALLLY RECEIVED BUNDLES ********/
void *inotify_thread()
{
	int wd = 0, fd_notify = 0;
	char buffer[INOTIFY_EVENT_BUF], full_path[PATH_MAX];
	char *p = NULL, *new_bundle_name = NULL;
	ssize_t buffer_l = 0;
	size_t input_path_l = 0;
	struct inotify_event *event = NULL;

	LOG_MSG(LOG__INFO, false, "Initiated inotify thread");

	if ((fd_notify = inotify_init()) < 0) {
		LOG_MSG(LOG__ERROR, true, "inotify_init()");
		goto end;
	}

	if ((wd = inotify_add_watch(fd_notify, g_vars.input_path, IN_CLOSE_WRITE)) < 0) {
		LOG_MSG(LOG__ERROR, true, "inotify_add_watch()");
		goto end;
	}

	LOG_MSG(LOG__INFO, false, "Processing stored bundles in %s", g_vars.input_path);
	if (process_multiple_bundles(g_vars.input_path) != 0)
		LOG_MSG(LOG__ERROR, false, "There has been an error processing the stored bundles");
	else
		LOG_MSG(LOG__INFO, false, "Stored bundles processed correctly");

	LOG_MSG(LOG__INFO, false, "Waiting for locally created bundles (in %s)", g_vars.input_path);

	input_path_l = strlen(g_vars.input_path);
	memcpy(full_path, g_vars.input_path, input_path_l);
	for (;;) {
		// Wait for a new bundle in the input folder
		buffer_l = read(fd_notify, buffer, INOTIFY_EVENT_BUF);
		if (buffer_l < 0) {
			LOG_MSG(LOG__ERROR, true, "inotify read()");
			continue;
		}

		// For each event
		for (p = buffer; p < buffer + buffer_l; ) {
			event = (struct inotify_event *) p;

			// Create full path
			new_bundle_name = event->name;
			snprintf(full_path + input_path_l, sizeof(full_path) - input_path_l, "/%s", new_bundle_name);

			LOG_MSG(LOG__INFO, false, "New bundle %s received", full_path);

			// Load and process bundle
			load_and_process_bundle(full_path);

			// Next event
			p += sizeof(struct inotify_event) + event->len;
		}
	}

end:
	close(fd_notify);
	kill(0, SIGINT);
	exit(0);
}

/***************/


/******* REMOTE RECEIVED BUNDLES *******/
static int get_and_process_bundle(int *sock)
{
	int ret = 1, off = 0;
	char *bundle_src_addr = NULL, agent_origin_id[MAX_PLATFORM_ID_LEN];
	uint16_t bundle_l = 0;
	ssize_t recv_ret = 0;
	uint8_t *bundle = NULL;
	struct sockaddr_in bundle_src = {0};
	socklen_t bundle_src_l = sizeof(bundle_src);
	unsigned recv_l = 0;

	// Get from where we are receiving the bundle
	if (getpeername(*sock, (struct sockaddr *)&bundle_src, &bundle_src_l) != 0) {
		LOG_MSG(LOG__ERROR, true, "getpeername()");
	} else {
		// If we ever change from AF_INET family, we should change this.
		bundle_src_addr = inet_ntoa(bundle_src.sin_addr);
	}
	LOG_MSG(LOG__INFO, false, "Receiving bundle form %s:%d", bundle_src_addr, ntohs(bundle_src.sin_port));

	// First we get the origin's platform id. We read until we find a NULL char.
	for (;;) {
		recv_ret = recv(*sock, agent_origin_id + off, sizeof(char), 0);
		if (recv_ret != sizeof(char)) {
			LOG_MSG(LOG__ERROR, true, "Error receiving origin's platform id from %s.", bundle_src_addr);
			goto end;
		}

		if (off + 1 == MAX_PLATFORM_ID_LEN) {
			LOG_MSG(LOG__ERROR, false, "Error receiving origin's platform id from %s. Origin's platform id too long.", bundle_src_addr);
			goto end;
		} else if (*(agent_origin_id + off) == '\0') {
			break;
		} else {
			off++;
		}
	}

	// We get the size of the bundle
	recv_ret = recv(*sock, &bundle_l, sizeof(bundle_l), 0);
	if (recv_ret != sizeof(bundle_l)) {
		if (recv_ret == 0) {
			LOG_MSG(LOG__ERROR, true, "Error receiving bundle length from %s. Probably peer has disconnected.", bundle_src_addr);
		} else if (recv_ret < 0) {
			LOG_MSG(LOG__ERROR, true, "Error receiving bundle length from %s.", bundle_src_addr);
		} else {
			LOG_MSG(LOG__ERROR, true, "Error receiving bundle length from %s. Length not in the correct format", bundle_src_addr);
		}
		goto end;
	}
	bundle_l = ntohs(bundle_l);

	// Then we receive the content of the bundle
	bundle = (uint8_t *)malloc(bundle_l * sizeof(uint8_t));
	while (recv_l != bundle_l) {
		recv_ret = recv(*sock, bundle + recv_l, bundle_l - recv_l, 0);
		if (recv_ret == -1) {
			LOG_MSG(LOG__ERROR, true, "Error receiving bundle from %s", bundle_src_addr);
			break;
		} else if (recv_ret == 0) {  // Peer closed the connection
			break;
		}
		recv_l += recv_ret;
	}

	if (recv_l != bundle_l) {
		LOG_MSG(LOG__ERROR, false, "Bundle not received correctly from %s.", bundle_src_addr);
		ret = 1;
		goto end;
	}

	LOG_MSG(LOG__INFO, false, "Recieved bundle form %s:%d with length %d", bundle_src_addr, ntohs(bundle_src.sin_port), recv_l);

	if (process_bundle(agent_origin_id, bundle, bundle_l) < 0) {
		LOG_MSG(LOG__ERROR, false, "Error processing bundle from %s", bundle_src_addr);
		goto end;
	}

	ret = 0;
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
	LOG_MSG(LOG__INFO, false, "Initiated socket thread");

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		LOG_MSG(LOG__ERROR, true, "socket()");
		ret = 1;
		goto end;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(g_vars.platform_port);
	serv_addr.sin_addr.s_addr = inet_addr(g_vars.platform_ip);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		LOG_MSG(LOG__ERROR, true, "bind()");
		ret = 1;
		goto end;
	}

	if (listen(sockfd, 10) != 0) {
		LOG_MSG(LOG__ERROR, true, "listen()");
		ret = 1;
		goto end;
	}

	LOG_MSG(LOG__INFO, false, "Waiting for connections");

	for (;;) {
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &cli_addr_l);
		if (newsockfd < 0 && errno == EINTR) {
			break;
		} else if (newsockfd < 0) {
			LOG_MSG(LOG__ERROR, true, "accept()");
			continue;
		}

		// Set timeout to socket
		if (setsockopt(newsockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval)) != 0)
			LOG_MSG(LOG__ERROR, true, "setsockopt()");

		newsockfd_t = (int *)malloc(sizeof(int));
		*newsockfd_t = newsockfd;

		if (pthread_create(&get_and_process_bundle_t, NULL, (void *) get_and_process_bundle, (void *) newsockfd_t) != 0) {
			LOG_MSG(LOG__ERROR, true, "pthread_create()");
			continue;
		}

		if (pthread_detach(get_and_process_bundle_t) != 0) {
			LOG_MSG(LOG__ERROR, true, "pthread_detach()");
			continue;
		}
	}

end:
	close(sockfd);
	exit(ret);
}
/***************/

int main(int argc, char *argv[])
{
	pthread_t inotify_t, sock_t;
	int sig = 0, ret = EXIT_FAILURE;
	struct common *shm;

	/* Initialization */
	sigset_t blocked_sigs = {{0}};
	if (sigaddset(&blocked_sigs, SIGINT) != 0)
		LOG_MSG(LOG__ERROR, true, "sigaddset()");
	if (sigaddset(&blocked_sigs, SIGTERM) != 0)
		LOG_MSG(LOG__ERROR, true, "sigaddset()");
	if (sigprocmask(SIG_BLOCK, &blocked_sigs, NULL) == -1)
		LOG_MSG(LOG__ERROR, true, "sigprocmask()");

	if (init_adtn_process(argc, argv, &shm) != 0) { //loads shared memory
		LOG_MSG(LOG__ERROR, false, "Error loading shared memory.");
		goto end;
	}

	// Reading from shm
	if (pthread_rwlock_rdlock(&shm->rwlock) != 0) {
		LOG_MSG(LOG__ERROR, true, "pthread_rwlock_rdlock()");
		goto end;
	}

	if (create_paths(shm->data_path) != 0) {
		LOG_MSG(LOG__ERROR, false, "Error creating all the required folders.");
		goto end;
	}

	g_vars.platform_ip = strdup(shm->iface_ip);
	g_vars.platform_id = strdup(shm->platform_id);
	g_vars.platform_port = shm->platform_port;
	g_vars.queue_conn = queue_manager_connect(shm->data_path, QUEUE_SOCKNAME);
	if (g_vars.queue_conn == 0) {
		LOG_MSG(LOG__ERROR, false, "Error connecting to the queue manager.");
		ret = EXIT_FAILURE;
		goto end;
	}

	if (pthread_rwlock_unlock(&shm->rwlock) != 0) {
		LOG_MSG(LOG__ERROR, true, "pthread_rwlock_unlock()");
		goto end;
	}
	// End reading from shm

	/**/

	/* Main threads */
	if (pthread_create(&inotify_t, NULL, inotify_thread, NULL) != 0) {
		LOG_MSG(LOG__ERROR, true, "pthread_create()");
		goto end;
	}
	if (pthread_create(&sock_t, NULL, socket_thread, NULL) != 0) {
		LOG_MSG(LOG__ERROR, true, "pthread_create()");
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
