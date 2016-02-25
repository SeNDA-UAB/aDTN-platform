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

#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <limits.h> //MAX NAME
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>

#include <openssl/md5.h>

#include "common/include/common.h"
#include "common/include/init.h"
#include "common/include/queue.h"
#include "common/include/executor.h"
#include "common/include/utils.h"

/* global vars*/
volatile sig_atomic_t working = 1;
struct common *shm;
char *bundle_queue;
char *own_ip;
char *own_id;
int proc_socket;
struct sockaddr_un exec_addr;
/**/

/* Defines */
#define QUEUE_SOCKNAME "/proc-queue.sock"
#define PROC_SOCKNAME "/proc-executor.sock"
#define EXEC_SOCKNAME "/executor.sock"
/**/
/* Structs*/
typedef struct {
	char neighbour[MAX_PLATFORM_ID_LEN];
	// char *bundle_id;
	uint8_t *raw_bundle;
	uint16_t raw_bundle_l;
} thread_sb_data;
/**/

void end_handler(int sign)
{
	if (sign == SIGINT)
		working = 0;
	signal(SIGINT, end_handler);
}

/* Processing functions */
static int process_code(int *num_hops, char *hops_list[MAX_HOPS_LEN], char *bundle_id)
{
	int ret = 1;
	int recv_l;
	struct _petition p = {.header = {0}};
	union _response r;

	p.header.petition_type = EXE;
	p.header.code_type = ROUTING_CODE;

	strncpy(p.header.bundle_id, bundle_id, NAME_MAX);
	LOG_MSG(LOG__INFO, false, "Executing routing code for bundle: %s", bundle_id);
	if (sendto(proc_socket, &p, sizeof(p), 0, (struct sockaddr *)&exec_addr, (socklen_t)sizeof(exec_addr)) < 0) {
		LOG_MSG(LOG__ERROR, true, "Unable to connect with executor");
		goto end;
	}
	recv_l = recv(proc_socket, &r, sizeof(r), 0);
	if (recv_l <= 0) {
		LOG_MSG(LOG__ERROR, true, "Unable to get executor response");
		goto end;
	}
	if (r.exec_r.correct == 1) {
		LOG_MSG(LOG__INFO, false, "Code correctly executed, the mssg should be send to %d nbs", r.exec_r.num_hops);
		*num_hops = r.exec_r.num_hops;
		*hops_list = (char *)malloc(*num_hops * MAX_PLATFORM_ID_LEN);
		memcpy(*hops_list, r.exec_r.hops_list, *num_hops * MAX_PLATFORM_ID_LEN);
		(*hops_list)[*num_hops * MAX_PLATFORM_ID_LEN - 1] = '\0';
	} else {
		*num_hops = -1;
		*hops_list = NULL;
	}
	ret = 0;
end:

	return ret;
}

static void delete_bundle(char *name)
{
	char *bundle_file = NULL;
	int len;

	len = strlen(bundle_queue) + strlen(name) + 2;
	bundle_file = (char *)calloc(len, sizeof(char));
	snprintf(bundle_file, len, "%s/%s", bundle_queue, name);
	if (remove(bundle_file) != 0)
		LOG_MSG(LOG__WARNING, false, "Can't delete bundle located in %s", bundle_file);
	else
		LOG_MSG(LOG__INFO, false, "Bundle %s has been deleted", bundle_file);
	free(bundle_file);
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

static int get_raw_bundle(char *name, uint8_t **raw_bundle)
{
	int ret = 1;
	int len = 0;
	char *bundle_file = NULL;

	len = strlen(bundle_queue) + strlen(name) + 2;
	bundle_file = (char *)calloc(len, sizeof(char));
	snprintf(bundle_file, len, "%s/%s", bundle_queue, name);

	ret = load_bundle(bundle_file, raw_bundle);

	if (bundle_file)free(bundle_file);

	return ret;
}

static int get_nb_ip_and_port(char *neighbour, char **ip, int *port)
{
	int ret = 1;

	if (get_nb_ip(neighbour, ip) != 0) {
		LOG_MSG(LOG__ERROR, false, "Error getting nb ip from RTI");
		goto end;
	}

	if (get_nb_port(neighbour, port) != 0) {
		LOG_MSG(LOG__ERROR, false, "Error getting nb port from RTI");
		goto end;
	}

	ret = 0;
end:

	return ret;
}


static int generate_unique_bundle_id(uint8_t *raw_bundle, /* out*/ unsigned char *digest)
{
	int ret = 1;
	uint64_t timestamp, timestamp_seq;
	uint8_t *bundle_src = NULL;
	char timestamp_string[64] = {0};
	MD5_CTX context;

	if (bunlde_raw_get_timestamp_and_seq(raw_bundle, &timestamp, &timestamp_seq) != 0) {
		LOG_MSG(LOG__ERROR, false, "Unable to generate unique bundle id: timestamp");
		goto end;
	}
	if (bundle_get_source(raw_bundle, &bundle_src) != 0) {
		LOG_MSG(LOG__ERROR, false, "Unable to generate unique bundle id: source");
		goto end;
	}

	snprintf(timestamp_string, 63, "%lu%lu", timestamp, timestamp_seq);

	MD5_Init(&context);
	MD5_Update(&context, (char *)bundle_src, strlen((char *)bundle_src));
	MD5_Update(&context, timestamp_string, strlen(timestamp_string));
	MD5_Final(digest, &context);
	ret = 0;
end:
	if (bundle_src)
		free(bundle_src);

	return ret;
}


static char *send_bundle_thread(thread_sb_data *data)
{
	int n_port, n, *ret;
	int sock  = 0;
	int st = 0;
	uint16_t bundle_l;
	char *n_ip = NULL;
	unsigned char digest[16] = {0};
	struct sockaddr_in remote_nb = {0};
	struct timeval timeout;
	char *bundle_src_addr = NULL;
	ret = (int *)malloc(sizeof(int));
	*ret = 0;

	if (get_nb_ip_and_port(data->neighbour, &n_ip, &n_port) != 0)
		goto end;
	if (generate_unique_bundle_id(data->raw_bundle, digest) != 0) {
		goto end;
	}
	// if(ban_sending((char*)digest, n_ip)){
	//  INFO_MSG("Bundle not send in roder to avoid cycles.");
	//  goto end;
	// }
	bzero((char *) &remote_nb, sizeof(char));
	remote_nb.sin_family = AF_INET;
	remote_nb.sin_port = htons(n_port);
	if (inet_aton(n_ip, &remote_nb.sin_addr) == 0) {
		LOG_MSG(LOG__ERROR, true, "Invalid nb address");
		goto end;
	}
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		LOG_MSG(LOG__ERROR, true, "Unable to create socket to contact nb");
		goto end;
	}
	timeout.tv_sec = NB_TIMEOUT / 1000000;
	timeout.tv_usec = NB_TIMEOUT - (timeout.tv_sec * 1000000);
	st -= setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
	st -= setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
	if (st != 0) {
		LOG_MSG(LOG__ERROR, true, "Can't set timeout for nb socket");
		goto end;
	}
	if (connect(sock, (struct sockaddr *)&remote_nb, sizeof(remote_nb)) < 0) {
		LOG_MSG(LOG__ERROR, true, "Can't connect with the neighbour");
		goto end;
	}
	if (data->raw_bundle_l <= 0) {
		LOG_MSG(LOG__ERROR, false, "Invalid bundle length");
		goto end;
	}
	bundle_l = htons(data->raw_bundle_l);

	n = write(sock, own_id, strlen(own_id) + 1); // Local platform id (NULL terminated)
	if (n < 0) {
		LOG_MSG(LOG__ERROR, false, "Can't write to the socket");
		goto end;
	}
	n = write(sock, &bundle_l, sizeof(uint16_t)); // Bundle length
	if (n < 0) {
		LOG_MSG(LOG__ERROR, false, "Can't write to the socket");
		goto end;
	}
	n = write(sock, data->raw_bundle, data->raw_bundle_l); // Bundle content
	if (n < 0) {
		LOG_MSG(LOG__ERROR, false, "Can't write to the socket");
		goto end;
	}

	struct sockaddr_in bundle_src = {0};
	socklen_t bundle_src_l = sizeof(bundle_src);
	// Get from where we are sending the bundle
	if (getsockname(sock, (struct sockaddr *)&bundle_src, &bundle_src_l) != 0) {
		LOG_MSG(LOG__ERROR, true, "getpeername()");
	} else {
		// If we ever change from AF_INET family, we should change this.
		bundle_src_addr = inet_ntoa(bundle_src.sin_addr);
	}

	LOG_MSG(LOG__INFO, false, "A bundle of length %d has been sent to %s:%d from %s:%d", data->raw_bundle_l, n_ip, n_port, bundle_src_addr, ntohs(bundle_src.sin_port));

	// add_contact((char*)digest, n_ip);

	*ret = 1;
end:
	if (n_ip)
		free(n_ip);
	if (sock)
		close(sock);
	free(data->raw_bundle);
	free(data);
	pthread_exit(ret);
}

static int send_bundle(char *bundle_name, char **neighbours, int n_hops, int *keep)
{
	int ret = 0, *t_ret;
	int wait_for = 0;
	int j = -1;
	int i;
	pthread_t *id_Threads;

	wait_for = n_hops;
	id_Threads = (pthread_t *)calloc(n_hops, sizeof(pthread_t));
	for (i = 0; i < n_hops; ++i) {
		if (strcmp(own_id, *neighbours + (i * MAX_PLATFORM_ID_LEN)) != 0) {
			++j;
			thread_sb_data *sb_data;
			sb_data = (thread_sb_data *)calloc(1, sizeof(thread_sb_data));
			strncpy(sb_data->neighbour, *neighbours + (i * MAX_PLATFORM_ID_LEN), sizeof(sb_data->neighbour));
			sb_data->raw_bundle_l = get_raw_bundle(bundle_name, &sb_data->raw_bundle);
			// len = strlen(bundle_name) + 1;
			// sb_data->bundle_id = (char*)calloc(len, sizeof(char));
			// strncpy(sb_data->bundle_id, bundle_name, len);
			if (pthread_create(&(id_Threads[j]), NULL, (void *)send_bundle_thread, (void *)sb_data) != 0) {
				LOG_MSG(LOG__ERROR, true, "Error creating thread to send bundle");
				--wait_for;
			}
		} else {
			LOG_MSG(LOG__INFO, false, "We are one of the bundles destination. We will keep a copy");
			*keep = 1;
			--wait_for;
		}
	}

	for (i = 0; i < wait_for; ++i) {
		pthread_join(id_Threads[i], (void **)&t_ret);
		ret += *t_ret;  //t_ret = 0 everything ok, neighbours++
		free(t_ret);
	}
	LOG_MSG(LOG__INFO, false, "The bundle has been sent to %d neighbours", ret);
	free(id_Threads);
	return ret;
}

static int bundle_file_exist(char *name)
{
	int ret = 1;
	int len = 0;
	char *bundle_file = NULL;
	FILE *f = NULL;

	len = strlen(bundle_queue) + strlen(name) + 2;
	bundle_file = (char *)calloc(len, sizeof(char));
	snprintf(bundle_file, len, "%s/%s", bundle_queue, name);

	f = fopen(bundle_file, "rb");
	if (f == NULL) {
		LOG_MSG(LOG__WARNING, false, "The bundle id %s doesn't exist", name);
		ret = 0;
	}
	free(bundle_file);
	if (f)
		fclose(f);
	return ret;
}

static void process_bundle(char *name, int queue_conn)
{
	int cod_r = 0;
	int n_hops = 0;
	int keep_bundle = 0;
	char *neighbours = NULL;
	int b_send = 0;

	LOG_MSG(LOG__INFO, false, "Start processing for bundle %s", name);
	if (bundle_file_exist(name) == 0)
		goto end;

	cod_r = process_code(&n_hops, &neighbours, name);
	if (n_hops <= 0 || cod_r != 0) {
		keep_bundle = 1;
		LOG_MSG(LOG__WARNING, false, "Unable to execute code for bundle %s", name);
		goto end;
	}
	b_send = send_bundle(name, &neighbours, n_hops, &keep_bundle);
	if (b_send != n_hops) {
		LOG_MSG(LOG__WARNING, false, "Bundle %s not sent to all nbs (%d/%d)", name, b_send, n_hops);
	}
	if (b_send == 0 && keep_bundle == 0)
		keep_bundle = 1;
end:
	if (keep_bundle) {
		queue(name, queue_conn);
		LOG_MSG(LOG__INFO, false, "Keeping a copy of %s bundle", name);
	} else {
		delete_bundle(name);
	}
	if (neighbours)free(neighbours);
}
/**/
/* Initialization and closing functions */
static int init_dirs()
{
	int len;
	int ret = 1;

	if (!shm->data_path)
		goto end;
	len = strlen(shm->data_path) + strlen(QUEUE_PATH) + 2;
	bundle_queue = (char *)calloc(len, sizeof(char));
	snprintf(bundle_queue, len, "%s/%s", shm->data_path, QUEUE_PATH);
	//creates directory if not exist
	mkdir(bundle_queue, 0755);
	if (errno != EEXIST && errno != 0)
		goto end;
	ret = 0;
end:

	return ret;
}

static int connect_executor()
{
	int ret = 0;
	char *sockname = NULL;
	char sockname_l = 0;
	char *exec_sockname = NULL;
	int exec_sockname_l = 0;
	struct sockaddr_un local_addr = {0};

	proc_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
	local_addr.sun_family = AF_UNIX;
	sockname_l = strlen(shm->data_path) + strlen(PROC_SOCKNAME) + 1;
	sockname = (char *)calloc(sockname_l, sizeof(char));
	snprintf(sockname, sockname_l, "%s%s", shm->data_path, PROC_SOCKNAME);
	unlink(sockname);
	strncpy(local_addr.sun_path, sockname, sizeof(local_addr.sun_path) - 1);

	if (bind(proc_socket, (struct sockaddr *)&local_addr, sizeof(struct sockaddr_un)) == -1) {
		LOG_MSG(LOG__ERROR, true, "[Processor] Creating executor socket");
		ret = 1;
		goto end;
	}

	exec_addr.sun_family = AF_UNIX;
	exec_sockname_l = strlen(shm->data_path) + strlen(EXEC_SOCKNAME) + 1;
	exec_sockname = (char *) calloc(exec_sockname_l, sizeof(char));
	snprintf(exec_sockname, exec_sockname_l, "%s%s", shm->data_path, EXEC_SOCKNAME);
	strncpy(exec_addr.sun_path, exec_sockname, sizeof(exec_addr.sun_path) - 1);
	free(exec_sockname);
end:
	free(sockname);

	return ret;
}

static void disconnect_executor()
{
	char *sockname = NULL;
	int sockname_l = 0;

	sockname_l = strlen(shm->data_path) + strlen(PROC_SOCKNAME) + 1;
	sockname = (char *)calloc(sockname_l, sizeof(char));
	snprintf(sockname, sockname_l, "%s%s", shm->data_path, PROC_SOCKNAME);

	close(proc_socket);
	unlink(sockname);
	free(sockname);
}

static int init_processor(int *queue_conn, int argc, char *argv[])
{
	int ret = 1;

	if (init_adtn_process(argc, argv, &shm) != 0) {
		LOG_MSG(LOG__ERROR, false, "Can't init shm");
		ret = -1;
		goto end;
	}
	own_ip = strdup(shm->iface_ip);
	own_id = strdup(shm->platform_id);

	if (init_dirs() != 0) {
		LOG_MSG(LOG__ERROR, true, "Can't create needed folders");
		goto end;
	}
	if (pthread_rwlock_wrlock(&shm->rwlock) != 0) {
		LOG_MSG(LOG__ERROR, true, "Can't block shm");
		goto end;
	}
	shm->pid_outgoing = getpid();
	pthread_rwlock_unlock(&shm->rwlock);

	*queue_conn = queue_manager_connect(shm->data_path, QUEUE_SOCKNAME);
	if (*queue_conn <= 0) {
		LOG_MSG(LOG__ERROR, true, "[Processor] Can't stablish connection with queueManager");
		goto end;
	}
	if (connect_executor() != 0) {
		LOG_MSG(LOG__ERROR, true, "Can't connect with executor");
		goto end;
	}
	LOG_MSG(LOG__INFO, false, "Processor initialized");
	ret = 0;
end:

	return ret;
}

static inline void close_processor(int queue_conn, int ret)
{
	if (bundle_queue)
		free(bundle_queue);
	if (ret == 0) {
		queue_manager_disconnect(queue_conn, shm->data_path, QUEUE_SOCKNAME);
		disconnect_executor();
	}
	if (ret > -1) {
		exit_adtn_process();
		free(own_ip);
	}
}
/**/

int main(int argc, char *argv[])
{
	int ret = 1;
	int queue_conn = 0;
	char *bundle_id;
	struct sigaction sa;

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = end_handler;
	if (sigaction(SIGINT, &sa, NULL) < 0) {
		LOG_MSG(LOG__ERROR, true, "[Processor] Can't set SIGINT handler");
		goto end;
	}
	bundle_queue = NULL;
	ret = init_processor(&queue_conn, argc, argv);
	if (ret != 0)
		goto end;

	while (working == 1) {
		bundle_id = dequeue(queue_conn);
		if (bundle_id == NULL) {
			usleep(WAIT_EMPTY_QUEUE);
			continue;
		}
		process_bundle(bundle_id, queue_conn);
		usleep(WAIT_FULL_QUEUE);
		free(bundle_id);
	}

	ret = 0;
end:
	close_processor(queue_conn, ret);

	return ret;
}
