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
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <code.h>
#include <tgmath.h>

#include "include/shm.h"
#include "include/log.h"
#include <math.h>

static char *get_shm_from_prefix(const unsigned prefix_id)
{
	char *shm_name = NULL;
	short int shm_name_l = strlen(SHM_MEM) + SHM_ID_LENGTH;

	if (prefix_id == 0)
		goto end;
	shm_name = (char *)malloc(shm_name_l);
	snprintf(shm_name, shm_name_l, "/%s-%x", SHM_MEM, prefix_id);
end:

	return shm_name;
}


int load_shared_memory_from_path(char *data_path, struct common **shm_common, int *shm_fd, bool write_permissions)
{

	int ret = -1;
	unsigned int shm_suffix = 0;

	if (data_path == NULL)
		goto end;
	shm_suffix = Crc32_ComputeBuf(shm_suffix, data_path, strlen(data_path));
	if (shm_suffix == 0)
		goto end;

	if ((*shm_fd = init_shared_memory(shm_suffix, write_permissions)) < 0) {
		LOG_MSG(LOG__ERROR, false, "Can't init shared memory");
		goto end;
	}
	*shm_common = NULL;
	if ((*shm_common = map_shared_memory(*shm_fd, write_permissions)) == NULL) {
		LOG_MSG(LOG__ERROR, false, "Can't map shared memory");
		goto end;
	}
	ret = 0;
end:

	return ret;
}

int init_shared_memory(const unsigned prefix_id, bool write_permissions)
{
	struct stat ms;
	int fd = 0;
	char *shm_name = NULL;

	shm_name = get_shm_from_prefix(prefix_id);

	//Init shared memory
	if (write_permissions)
		fd = shm_open(shm_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); //chmod = 664
	else
		fd = shm_open(shm_name, O_RDONLY, S_IRUSR | S_IRGRP | S_IROTH); //chmod = 444
	if (fd == -1) {
		if (write_permissions)
			LOG_MSG(LOG__ERROR, true, "Error creating or opening an existing shared memory with write permissions");
		else
			LOG_MSG(LOG__ERROR, true, "Error opening an existing shared memory with read permissions");
		goto end;
	}

	if (fstat(fd, &ms) != 0) {
		LOG_MSG(LOG__ERROR, true, "fstat()");
		goto end;
	}

	if (ms.st_size == 0 || ms.st_size != sizeof(struct common)) {
		ftruncate(fd, sizeof(struct common));
		struct common *addr;
		addr = (struct common *)mmap(NULL, sizeof(struct common), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		bzero(addr, sizeof(struct common));
	}

end:
	if (shm_name)
		free(shm_name);

	return fd;
}

struct common *map_shared_memory(int shm_fd, bool write_permissions)
{
	struct common *addr = NULL;

	if (write_permissions)
		addr = (struct common *)mmap(NULL, sizeof(struct common), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	else
		addr = (struct common *)mmap(NULL, sizeof(struct common), PROT_READ, MAP_SHARED, shm_fd, 0);
	if (addr == MAP_FAILED) {
		if (write_permissions)
			LOG_MSG(LOG__ERROR, true, "Error mapping shared memory with write permissions");
		else
			LOG_MSG(LOG__ERROR, true, "Error mapping shared memory with read permissions");
		addr = NULL;
	}
	return addr;
}

int unmap_shared_memory(struct common *addr)
{
	int ret = 1;
	if (!addr)
		goto end;
	ret = munmap(addr, sizeof(struct common));
end:

	return ret;
}

int remove_shared_memory(const unsigned int prefix_id)
{
	char *shm_name = get_shm_from_prefix(prefix_id);
	int ret = shm_unlink(shm_name);

	free(shm_name);
	return ret;
}
