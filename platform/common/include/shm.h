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
