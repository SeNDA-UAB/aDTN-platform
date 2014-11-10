#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/un.h>
#include <pthread.h>

#include "common/include/log.h"
#include "common/include/init.h"
#include "common/include/bundle.h"
#include "common/include/executor.h"
#include "common/include/constants.h"

#define DEF_SOCKNAME "/queue_manager.sock"
#define QUEUE_SOCKNAME "%s/executor_queue.sock"
#define EXEC_SOCKNAME "%s/executor.sock"

#define EXEC_T 0x00

#define INFO_MSG(...) do {if (DEBUG) info_msg(__VA_ARGS__);} while(0);

typedef struct node {
	char *bundle_id;
	int prio;
	struct node *next;
	struct node *previous;
} node_t;

typedef struct queue {
	node_t *head;
	node_t *tail;
	node_t *scheduled;
	int size;
} queue_t;

/* global vars */
queue_t queue = {0};
volatile sig_atomic_t working = 1;
pthread_mutex_t queue_mutex;
int queue_socket;
struct sockaddr_un exec_addr = {0};
struct common *shm = NULL;
/**/

static void sigint_handler(int sign)
{
	if (sign == SIGINT)
		working = 0;
	signal(SIGINT, sigint_handler);
}

/** Queue functions **/
static int insert(queue_t *queue, char *bundle_id, int prio)
{

	int len;
	node_t *iterator;
	node_t *new_node;

	if (!bundle_id || strcmp(bundle_id, "") == 0)
		return 1;
	new_node = calloc(1, sizeof(node_t));
	len = strlen(bundle_id) + 1;
	new_node->bundle_id = calloc(len, sizeof(char));
	strncpy(new_node->bundle_id, bundle_id, len);
	new_node->prio = prio;

	if (queue->head == NULL) {
		new_node->previous = NULL;
		new_node->next = NULL;
		queue->scheduled = new_node;
		queue->head = new_node;
		queue->tail = new_node;
		queue->size++;
		return 0;
	}

	iterator = queue->head;
	while ((iterator != NULL) && (iterator->prio > prio)) {
		iterator = iterator->next;
	}

	if (iterator == queue->head) {
		new_node->next = queue->head;
		new_node->previous = NULL;
		queue->head->previous = new_node;
		queue->head = new_node;
	} else if (iterator == queue->tail) {
		new_node->next = queue->tail;
		new_node->previous = queue->tail->previous;
		queue->tail->previous->next = new_node;
		queue->tail->previous = new_node;
	} else if (iterator == NULL) {
		new_node->previous = queue->tail;
		queue->tail->next = new_node;
		queue->tail = new_node;
	} else {
		new_node->previous = iterator->previous;
		new_node->next = iterator;
		iterator->previous->next = new_node;
		iterator->previous = new_node;
	}
	queue->size++;

	return 0;
}

static int dequeue_scheduled(queue_t *queue, char **bundle_id)
{
	int len;
	node_t *aux;

	if (queue->head == NULL) {
		*bundle_id = NULL;
		return 0;
	}
	if (queue->scheduled == NULL) {
		queue->scheduled = queue->head;
	}

	len = strlen(queue->scheduled->bundle_id) + 1;
	*bundle_id = calloc(len, sizeof(char));
	strncpy(*bundle_id, queue->scheduled->bundle_id, len);

	if (queue->scheduled == queue->head) {
		if (queue->head != queue->tail) {
			queue->head = queue->head->next;
			queue->head->previous = NULL;
		} else {
			queue->head = NULL;
		}
	} else if (queue->scheduled == queue->tail) {
		queue->tail = queue->tail->previous;
		queue->tail->next = NULL;
	} else {
		queue->scheduled->previous->next = queue->scheduled->next;
		queue->scheduled->next->previous = queue->scheduled->previous;
	}

	aux = queue->scheduled;
	queue->scheduled = queue->scheduled->next;
	if (aux) {
		free(aux->bundle_id);
		free(aux);
	}
	queue->size--;

	return 0;
}

static int enqueue(queue_t *queue, char *bundle_id)
{
	int ret = 1;
	int len;
	node_t *new_node = NULL;

	if (!bundle_id || strcmp(bundle_id, "") == 0)
		goto end;
	new_node = calloc(1, sizeof(node_t));
	len = strlen(bundle_id) + 1;
	if (!new_node)
		goto end;
	new_node->bundle_id = calloc(len, sizeof(char));
	new_node->prio = -1;
	if (!new_node->bundle_id)
		goto end;
	strncpy(new_node->bundle_id, bundle_id, len);
	new_node->next = NULL;

	if (queue->head == NULL) {
		queue->head = new_node;
		new_node->previous = NULL;
	} else {
		new_node->previous = queue->tail;
		queue->tail->next = new_node;
	}
	new_node->next = NULL;
	queue->tail = new_node;
	queue->size++;
	if (queue->scheduled == NULL)
		queue->scheduled = queue->tail;
	ret = 0;
end:
	if (new_node != NULL && ret == 1)
		free(new_node);

	return 0;
}

static int dequeue(queue_t *queue, char **bundle_id)
{
	int len;
	node_t *aux;

	if (queue->head == NULL) {
		*bundle_id = NULL;
		return 0;
	}
	len = strlen(queue->head->bundle_id) + 1;
	*bundle_id = calloc(len, sizeof(char));
	if (!bundle_id)
		return 1;
	strncpy(*bundle_id, queue->head->bundle_id, len);
	if (queue->scheduled == queue->head)
		queue->scheduled = queue->head->next;
	aux = queue->head;
	if (queue->head->next != NULL) {
		queue->head = queue->head->next;
		queue->head->previous = NULL;
	} else {
		queue->head = NULL;
	}
	free(aux->bundle_id);
	free(aux);
	queue->size--;

	return 0;
}

static inline int get_queue_size(queue_t *queue)
{
	return queue->size;
}

static void empty_queue(queue_t *queue)
{
	node_t *iterator;
	while (queue->head) {
		iterator = queue->head;
		queue->head = queue->head->next;
		queue->size--;
		free(iterator->bundle_id);
		free(iterator);
	}
}

static int file_select( const struct dirent *d)
{
	if (d->d_type == DT_REG)
		return 1;
	else
		return 0;
}

int queue_init(queue_t *queue, char *path)
{
	int n_bundles;
	struct dirent **bundle_list;

	n_bundles = scandir(path, &bundle_list, file_select, NULL);
	if (n_bundles < 0)
		return 1;
	while (n_bundles--) {
		enqueue(queue, bundle_list[n_bundles]->d_name);
		free(bundle_list[n_bundles]);
	}
	free(bundle_list);

	return 0;
}
/**/

/** Helper functions **/
static int connect_executor(char *data_path)
{
	int ret = 1;
	char *sockname = NULL;
	int sockname_l = 0;
	struct timeval timeout;

	queue_socket = 0;
	queue_socket = socket(AF_UNIX, SOCK_DGRAM, 0);

	struct sockaddr_un client_addr = {0};
	client_addr.sun_family = AF_UNIX;

	sockname_l = strlen(data_path) + strlen(QUEUE_SOCKNAME) + 1;
	sockname = malloc(sockname_l);
	snprintf(sockname, sockname_l, QUEUE_SOCKNAME, data_path);
	unlink(sockname); //Prevention
	strncpy(client_addr.sun_path, sockname , sizeof(client_addr.sun_path) - 1);

	if (bind(queue_socket, (struct sockaddr *) &client_addr, sizeof(struct sockaddr_un)) == -1) {
		perror("Procesor bind");
		goto end;
	}

	timeout.tv_sec = EXEC_TIMEOUT / 1000000;
	timeout.tv_usec = EXEC_TIMEOUT - (timeout.tv_sec * 1000000);
	//setsockopt(queue_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
	//setsockopt(queue_socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
	ret = 0;
end:
	if (sockname)
		free(sockname);

	return ret;
}

static void disconnect_executor(char *data_path)
{
	char *sockname;
	int sockname_l = 0;

	sockname_l = strlen(data_path) + strlen(QUEUE_SOCKNAME) + 1;
	sockname = malloc(sockname_l);
	snprintf(sockname, sockname_l, QUEUE_SOCKNAME, data_path);

	close(queue_socket); //closing executior connection
	unlink(sockname);
	free(sockname);
}

static void compose_exec_addr(char *data_path)
{
	char *exec_sockname;
	int exec_sockname_l;

	exec_sockname_l = strlen(data_path) + strlen(EXEC_SOCKNAME) + 1;
	exec_sockname = malloc(exec_sockname_l);
	snprintf(exec_sockname, exec_sockname_l, EXEC_SOCKNAME, data_path);

	exec_addr.sun_family = AF_UNIX;
	strncpy(exec_addr.sun_path, exec_sockname , sizeof(exec_addr.sun_path) - 1);
	free(exec_sockname);
}

/* Run code */
static int run_code(char *bundle_id, code_type_e ct)
{
	int ret = 0;
	int recv_l;

	struct _petition p = {.header = {0}};
	struct _petition rp = {.header = {0}};
	union _response r = {.exec_r.header = {0}};
	union _response rr = {.exec_r.header = {0}};

	p.header.petition_type = EXE; //Execution
	p.header.code_type = ct;
	strncpy(p.header.bundle_id, bundle_id, NAME_MAX); //Bundle id

	if (sendto(queue_socket, &p, sizeof(p), 0, (struct sockaddr *)&exec_addr, (socklen_t)sizeof(exec_addr)) == -1) {
		err_msg(true, "Unable to contact executor");
		goto end;
	}
	recv_l = recv(queue_socket, &r, sizeof(r), 0);
	if (recv_l <= 0) {
		err_msg(true, "[NORMAL] No response from executor");
		ret = 0;
		goto end;
	}
	if (r.simple.result == 1 && ct == LIFE_CODE) { // If remove bundle
		rp.header.petition_type = RM;
		rp.header.code_type = ct;
		strncpy(rp.header.bundle_id, bundle_id, NAME_MAX); //Bundle id
		sendto(queue_socket, &rp, sizeof(rp), 0, (struct sockaddr *)&exec_addr, (socklen_t)sizeof(exec_addr));
		recv_l = recv(queue_socket, &rr, sizeof(rr), 0);
		if (recv_l <= 0) {
			err_msg(true, "[ERASE] No response from executor");
			ret = 0;
			goto end;
		}
	}

	ret = r.simple.correct ? r.simple.result : 0;
end:

	return ret;
}
/**/
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

static int get_raw_bundle(char *name, uint8_t **raw_bundle)
{
	int ret = 1;
	char *bundle_path;
	asprintf(&bundle_path, "%s/%s/%s", shm->data_path, QUEUE_PATH, name);

	ret = load_bundle(bundle_path, raw_bundle);

	free(bundle_path);
	return ret;
}

static int is_lifetime_expired(char *bundle_id)
{
	int ret = 0;
	time_t t_now;
	uint8_t *raw_bundle;
	uint64_t timestamp, timestamp_seq, lifetime;

	t_now = time(NULL);
	if (get_raw_bundle(bundle_id, &raw_bundle) <= 0)
		goto end;
	if (bunlde_raw_get_timestamp_and_seq(raw_bundle, &timestamp, &timestamp_seq) != 0)
		goto end;
	if (bundle_raw_get_lifetime(raw_bundle, &lifetime) != 0)
		goto end;
	if (timestamp + lifetime + RFC_DATE_2000 < t_now) {
		INFO_MSG("Checking lifetime. Bundle's timestamp: %lu,  lifetime %lu. Local time: %lu ", timestamp + RFC_DATE_2000, lifetime, t_now);
		ret = 1;
	}
end:

	return ret;
}

static int schedule(char *path)
{
	int ret = 1, del_life = 0, del_life_code = 0;
	int len;
	int prio = 0;
	char *bundle_id;
	char *bundle_path;

	dequeue_scheduled(&queue, &bundle_id);
	if (!bundle_id)
		goto end;

	if (is_lifetime_expired(bundle_id))
		del_life = 1;
	else if (run_code(bundle_id, LIFE_CODE))
		del_life_code = 1;

	if (del_life || del_life_code) {
		len = strlen(bundle_id) + 1 + strlen(path) + 1;
		bundle_path = (char *)calloc(len, sizeof(char));
		snprintf(bundle_path, len, "%s/%s", path, bundle_id);
		remove(bundle_path);

		if (del_life)
			INFO_MSG("Bundle %s removed because its lifetime expired", bundle_path);
		if (del_life_code)
			INFO_MSG("Bundle %s removed because of its lifetime code", bundle_path);

		free(bundle_path);
	} else {
		prio = run_code(bundle_id, PRIO_CODE);
		insert(&queue, bundle_id, prio);
	}

	free(bundle_id);
	ret = 0;
end:

	return ret;
}

void *scheduler_thread(void *path)
{
	int size;
	char *bundles_queue_path = (char *)path;
	while (working == 1) {
		size = get_queue_size(&queue);
		if (size > 0) {
			pthread_mutex_lock(&queue_mutex);
			schedule(bundles_queue_path);
			pthread_mutex_unlock(&queue_mutex);
			usleep(SCHEDULER_TIMER / size);
		} else {
			usleep(SCHEDULER_TIMER);
		}
	}
	free(bundles_queue_path);
	pthread_exit((void *)0);
}

void petitions_handler(int sock, char *str, struct sockaddr_un remote)
{
	pthread_mutex_lock(&queue_mutex);
	if (strncmp(str, "01", 2) == 0) {
		enqueue(&queue, str + 2);
	} else if (strncmp(str, "00", 2) == 0) {
		char *bundle_id;
		dequeue(&queue, &bundle_id);
		if (bundle_id) {
			sendto(sock, bundle_id, strlen(bundle_id), 0, (struct sockaddr *)&remote, (socklen_t)sizeof(remote));
			free(bundle_id);
		} else {
			bundle_id = strdup("NULL");
			sendto(sock, bundle_id, strlen(bundle_id), 0, (struct sockaddr *)&remote, (socklen_t)sizeof(remote));
			free(bundle_id);
		}
	} else if (strncmp(str, "10", 2) == 0) {
		int size;
		size = get_queue_size(&queue);
		memset(str, 0, MAX_BUFFER);
		snprintf(str, MAX_BUFFER, "%d", size);
		sendto(sock, str, strlen(str), 0, (struct sockaddr *)&remote, (socklen_t)sizeof(remote));
	} else if (strncmp(str, "11", 2) == 0) {
		empty_queue(&queue);
	}
	pthread_mutex_unlock(&queue_mutex);
}

void *process_thread()
{
	int len;
	int received;
	int sock;
	char str[MAX_BUFFER];
	struct sockaddr_un local, remote = {0};
	size_t sock_path_l = 0;
	struct timeval timeout;
	socklen_t remote_addr_l = sizeof(remote);

	if ((sock = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
		perror("");
	local.sun_family = AF_UNIX;
	sock_path_l = strlen(shm->data_path) + strlen(DEF_SOCKNAME) + 1;
	snprintf(local.sun_path, sock_path_l, "%s%s", shm->data_path, DEF_SOCKNAME);
	unlink(local.sun_path);

	len = sizeof(local);
	if (bind(sock, (struct sockaddr *)&local, len) == -1)
		perror("");

	timeout.tv_sec = PROC_TIMEOUT / 1000000;
	timeout.tv_usec = PROC_TIMEOUT - (timeout.tv_sec * 1000000);
	//setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
	//setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));

	while (working == 1) {
		memset(str, 0, MAX_BUFFER);
		received = recvfrom(sock, str, MAX_BUFFER, 0, (struct sockaddr *)&remote, (socklen_t *)&remote_addr_l);
		if (received <= 0) {
			continue;
		}
		petitions_handler(sock, str, remote);
	}
	unlink(local.sun_path);
	close(sock);
	pthread_exit((void *)0);
}

int main(int argc, char *const argv[])
{
	int len;
	char *path; // Valgrind will indicates one more alloc than frees due to this variable. It is freed in scheduler_thread, dont free again!
	pthread_attr_t tattr;
	pthread_t proc_thread;
	pthread_t sche_thread;

	pthread_mutex_init(&queue_mutex, NULL);
	if (init_adtn_process(argc, argv, &shm) != 0) {
		err_msg(false, "Process initialization failed. Aborting execution");
		exit_adtn_process();
		exit(1);
	}

	struct sigaction act = {{0}};
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGINT);
	act.sa_handler = sigint_handler;
	if (sigaction(SIGINT, &act, NULL) != 0)
		return 1;
	signal(SIGPIPE, SIG_IGN);
	len = strlen(shm->data_path) + 1 + strlen(QUEUE_PATH);
	path = calloc(len, sizeof(char));
	snprintf(path, len, "%s/"QUEUE_PATH, shm->data_path);
	queue_init(&queue, path);

	connect_executor(shm->data_path);
	compose_exec_addr(shm->data_path);

	printf("We have %d bundles in queue\n", get_queue_size(&queue));
	fflush(0);

	pthread_attr_init(&tattr);
	pthread_create(&proc_thread, &tattr, process_thread, NULL);
	pthread_create(&sche_thread, &tattr, scheduler_thread, (void *)path);

	pthread_join(proc_thread, NULL);
	pthread_join(sche_thread, NULL);

	empty_queue(&queue);
	disconnect_executor(shm->data_path);
	pthread_mutex_destroy(&queue_mutex);
	exit_adtn_process();
	return 0;
}
