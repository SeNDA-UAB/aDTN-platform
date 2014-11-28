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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "log.h"
#include "paths.h"
#include "constants.h"


struct sockaddr_un queue_addr = {0};

static void compose_queue_addr(char *data_path)
{
	char *queue_sockname;
	int queue_sockname_l;

	queue_sockname_l = strlen(data_path) + strlen(SOCK_PATH) + 1;
	queue_sockname = (char *)malloc(queue_sockname_l);
	snprintf(queue_sockname, queue_sockname_l, "%s%s", data_path, SOCK_PATH);

	queue_addr.sun_family = AF_UNIX;
	strncpy(queue_addr.sun_path, queue_sockname , sizeof(queue_addr.sun_path) - 1);
	free(queue_sockname);
}

static char *send_petition(char *msg, int answer, int sock)
{
	char str[MAX_BUFFER] = {0};

	if (sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&queue_addr, (socklen_t)sizeof(queue_addr)) < 0) {
		LOG_MSG(LOG__ERROR, true, "[Queue]Unable to contact");
		goto end;
	}
	if (answer) {
		int received = recv(sock, str, MAX_BUFFER, 0);
		if (received <= 0) {
			LOG_MSG(LOG__ERROR, true, "[Queue] Unable to get response");
		}
		char *bundle_id = ((strcmp(str, "NULL") != 0) && (received > 0)) ? strdup(str) : NULL;
		return bundle_id;
	}
end:

	return NULL;
}

int queue_manager_connect(char *data_path, char *q_sockname)
{
	int sock, len;
	struct sockaddr_un local;
	struct timeval timeout;

	if ((sock =  socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
		return 0;
	}

	/* NEW */
	local.sun_family = AF_UNIX;
	len = strlen(data_path) + strlen(q_sockname) + 1;
	snprintf(local.sun_path, len, "%s%s", data_path, q_sockname);
	unlink(local.sun_path);
	len = sizeof(local);
	if (bind(sock, (struct sockaddr *)&local, len) == -1) {
		perror("Queue Bind");
		return 0;
	}

	timeout.tv_sec = CONN_TIMEOUT / 1000000;
	timeout.tv_usec = CONN_TIMEOUT - (timeout.tv_sec * 1000000);
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
	/**/
	compose_queue_addr(data_path);

	return sock;
}

inline void queue_manager_disconnect(int queue_conn, char *data_path, char *q_sockname)
{
	char *sockname;
	int len;

	close(queue_conn);
	len = strlen(data_path) + strlen(q_sockname) + 1;
	sockname = (char *)calloc(len, sizeof(char));
	snprintf(sockname, len, "%s%s", data_path, q_sockname);
	unlink(sockname);
	free(sockname);
}

int queue(char *bundle_id, int queue_conn)
{
	int len;
	int ret = 1;
	char *msg;

	if (queue_conn <= 0)
		goto end;
	if (bundle_id == NULL) {
		goto end;
	}

	len = 2 + strlen(bundle_id) + 1;
	msg = (char *)calloc(len, 1);
	snprintf(msg, len, "01%s", bundle_id);
	send_petition(msg, 0, queue_conn);
	free(msg);

	ret = 0;
end:

	return ret;
}

char *dequeue(int queue_conn)
{
	char msg[3] = {0};
	char *bundle_id;

	if (queue_conn <= 0)
		return NULL;
	strncpy(msg, "00", 2);
	bundle_id = send_petition(msg, 1, queue_conn);

	return bundle_id;
}

int bundles_in_queue(int queue_conn)
{
	char msg[3] = {3};
	char *bundle_id;
	int nBundles;

	if (queue_conn <= 0)
		return -1;
	strncpy(msg, "10", 2);
	nBundles = atoi(send_petition(msg, 1, queue_conn));

	return nBundles;
}

void empty_queue(int queue_conn)
{
	char msg[3] = {0};

	if (queue_conn <= 0)
		return;
	strncpy(msg, "11", 2);
	send_petition(msg, 0, queue_conn);
}