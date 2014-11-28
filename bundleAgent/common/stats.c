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
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <float.h>

#include "include/log.h"
#include "constants.h"


typedef enum {READ, WRITE} action_t;

struct adtn_stat {
	char value[10]; //Max precission
};

struct adtn_stat *open_adtn_stat(const char *stat, action_t action)
{
	if (stat == NULL || *stat == '\0') {
		LOG_MSG(LOG__ERROR, true, "open_adtn_stat(): Invalid path");
		return NULL;
	}

	char full_path[1024];
	int full_path_l;
	full_path_l = snprintf(full_path, 1024, "/%s", stat);
	if (full_path_l < 0) {
		LOG_MSG(LOG__ERROR, true, "open_adtn_stat(): snprintf()");
		return NULL;
	} else if (full_path_l > 1024) {
		LOG_MSG(LOG__ERROR, true, "open_adtn_stat(): path to long");
		return NULL;
	}

	int fd;
	if (action == WRITE) {
		fd = shm_open(full_path, O_CREAT | O_RDWR, PERMISSIONS);
	} else {
		fd = shm_open(full_path, O_RDONLY, 0);
	}
	if (fd == -1) {
		LOG_MSG(LOG__ERROR, true, "open_adtn_stat(): shm_open()");
		return NULL;
	}

	if (action == WRITE) {
		struct stat ms;
		if (fstat(fd, &ms) != 0) {
			close(fd);
			LOG_MSG(LOG__ERROR, true, "open_adtn_stat(): fstat():");
			return NULL;
		}
		if (ms.st_size == 0) {
			if (ftruncate(fd, sizeof(struct adtn_stat)) != 0) {
				close(fd);
				LOG_MSG(LOG__ERROR, true, "open_adtn_stat(): ftruncate()");
				return NULL;
			}
		}
	}

	struct adtn_stat *new_adtn_stat;
	if (action == WRITE)
		new_adtn_stat = mmap(NULL, sizeof(struct adtn_stat), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	else
		new_adtn_stat = mmap(NULL, sizeof(struct adtn_stat), PROT_READ, MAP_SHARED, fd, 0);
	if (new_adtn_stat == MAP_FAILED) {
		close(fd);
		LOG_MSG(LOG__ERROR, true, "set_stat(): mmap():");
		return NULL;
	}

	close(fd);

	return new_adtn_stat;
}

int close_adtn_stat(struct adtn_stat *stat_to_unmap)
{
	if (munmap(stat_to_unmap, (sizeof(struct adtn_stat))) != 0) {
		LOG_MSG(LOG__ERROR, true, "close_adtn_stat(): munmap():");
		return 1;
	}

	return 0;
}

float get_stat(const char *stat)
{
	struct adtn_stat *stat_to_get = open_adtn_stat(stat, READ);
	if (stat_to_get != NULL) {
		float stat_value = strtof(stat_to_get->value, NULL);
		if (stat_value > FLT_MAX || stat_value < -FLT_MAX) {
			LOG_MSG(LOG__ERROR, true, "get_stat(): Stored value too big");
			close_adtn_stat(stat_to_get);
			return -2;
		} else {
			close_adtn_stat(stat_to_get);
			return stat_value;
		}
	} else {
		return -1;
	}
}

int set_stat(const char *stat, const float new_value)
{
	struct adtn_stat *stat_to_up = open_adtn_stat(stat, WRITE);
	if (stat_to_up != NULL) {
		snprintf(stat_to_up->value, 10, "%f", new_value);
		stat_to_up->value[9] = '\0';
		close_adtn_stat(stat_to_up);
		return 0;
	} else {
		return 1;
	}
}

int increase_stat(const char *stat)
{
	struct adtn_stat *stat_to_up = open_adtn_stat(stat, WRITE);
	if (stat_to_up != NULL) {
		float old_value_float = strtof(stat_to_up->value, NULL);
		old_value_float++;
		snprintf(stat_to_up->value, 10, "%f", old_value_float);
		stat_to_up->value[9] = '\0';
		close_adtn_stat(stat_to_up);
		return 0;
	} else {
		return 1;
	}
}

int decrease_stat(const char *stat)
{
	struct adtn_stat *stat_to_up = open_adtn_stat(stat, WRITE);
	if (stat_to_up != NULL) {
		float old_value_float = strtof(stat_to_up->value, NULL);
		old_value_float--;
		snprintf(stat_to_up->value, 10, "%f", old_value_float);
		stat_to_up->value[9] = '\0';
		close_adtn_stat(stat_to_up);
		return 0;
	} else {
		return 1;
	}
	return 0;
}

int reset_stat(char *stat)
{
	struct adtn_stat *stat_to_up = open_adtn_stat(stat, WRITE);
	if (stat_to_up != NULL) {
		snprintf(stat_to_up->value, 10, "%f", 0.0);
		stat_to_up->value[9] = '\0';
		close_adtn_stat(stat_to_up);
		return 0;
	} else {
		return 1;
	}

}

int remove_stat(const char *stat)
{
	char full_path[1042];
	int full_path_l;
	full_path_l = snprintf(full_path, 1024, "%s%s", BASE_PATH, stat);
	if (full_path_l < 0) {
		LOG_MSG(LOG__ERROR, true, "open_adtn_stat(): snprintf()");
		return 1;
	} else if (full_path_l > 1024) {
		LOG_MSG(LOG__ERROR, false, "open_adtn_stat(): path to long");
		return 1;
	}

	if (shm_unlink(full_path) == -1) {
		LOG_MSG(LOG__ERROR, true, "remove_stat(): Can't remove stat");
		return 1;
	} else {
		return 0;
	}
}


float calculate_ewma(const float old_ewma, const float new_value, const float factor)
{
	if (old_ewma == 0)
		return new_value;
	else
		return factor * new_value + (1.0 - factor) * old_ewma;
}

