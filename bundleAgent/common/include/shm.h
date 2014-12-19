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

#ifndef INC_COMMON_SHM_H
#define INC_COMMON_SHM_H

#include <semaphore.h>
#include <limits.h>
#include <stdbool.h>

#include "constants.h"

//Be careful when using dynamic memory allocation inside the shared memory! All have to be copied into the shared memory section!
struct common {
	int initialized;
	char iface_ip[STRING_IP_LEN];
    char platform_id[MAX_PLATFORM_ID_LEN];
	short int platform_port;
	unsigned int prefix_id;
	char data_path[PATH_MAX];
	char config_file[PATH_MAX];

	pthread_rwlock_t rwlock;
	int pid_outgoing;
};

int load_shared_memory_from_path(char *data_path, struct common **shm_common, int *shm_fd, bool write_permissions);
int init_shared_memory(const unsigned int prefix_id, bool write_permissions);
struct common *map_shared_memory(int shm_fd, bool write_permissions);
int unmap_shared_memory(struct common* addr);
int remove_shared_memory(const unsigned int prefix_id);

#endif
