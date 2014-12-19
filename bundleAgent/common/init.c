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
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <semaphore.h>
#include <time.h>
#include <unistd.h>
#include <sys/file.h>
#include <limits.h>

#include "include/init.h"
#include "include/config.h"
#include "include/rit.h"
#include "include/log.h"
#include "include/minIni.h"
#include "include/code.h"

static int shm_fd = 0;

static void help(char *program_name)
{
	printf( "%s is part of the SeNDA aDTN platform\n"
	        "Usage: %s [options]\n"
	        "Supported options:\n"
	        "       -c [config_file]      %s will use the specified config file instead of the default found at "DEFAULT_CONF_FILE_PATH"\n"
	        "       -f            Force to reread the config file\n"
	        "       -h            Shows this help message\n"
	        , program_name, program_name, program_name);
}

static int parse_adtn_options(int argc, char *const argv[], char **config_dir, short int *force_init)
{
	int opt, option_index = 0, ret = 0;
	static struct option long_options[] = {
		{"help",    no_argument,            0,      'h'},
		{"config",  required_argument,      0,      'c'},
		{"force_init",  no_argument,        0,      'f'},
		{0, 0, 0, 0}
	};
	if (argc < 0) {
		ret = 1;
		goto end;
	}
	while ((opt = getopt_long(argc, argv, "fhc:", long_options, &option_index))) {
		switch (opt) {
		case 'c':
			*config_dir = strdup(optarg);
			break;
		case 'f':
			*force_init = 1;
			break;
		case 'h':
			help(argv[0]);
			exit(0);
		// case '?':           //Unexpected parameter
		//  help(argv[0]);
		//  ret = 1;
		default:
			goto end;
		}
	}
end:
	optind = 1;

	return ret;
}

int try_sem(sem_t *semaphore)
{
	int ret = 0;
	struct timespec wait_time;
	wait_time.tv_nsec = 0;

	if (sem_trywait(semaphore) != 0 && (errno == EAGAIN)) {
		LOG_MSG(LOG__WARNING, true, "Queue semaphore locked. Uncontrolled exit?. Waiting 5 seconds before resetting it.");
		wait_time.tv_sec = time(NULL) + 5;
		if (sem_timedwait(semaphore, &wait_time) != 0 && (errno == ETIMEDOUT || errno == EDEADLK)) {
			LOG_MSG(LOG__WARNING, false, "Can't get queue semaphore. Resetting it");
			if (sem_destroy(semaphore) != 0) {
				LOG_MSG(LOG__ERROR, true, "sem_destroy()");
				ret = 1;
				goto end;
			}
			if (sem_init(semaphore, 1, 1) != 0) {
				LOG_MSG(LOG__ERROR, true, "sem_init()");
				ret = 1;
				goto end;
			}
		} else if (errno == EINVAL) {
			LOG_MSG(LOG__ERROR, true, "sem_timedwait()");
			ret = 1;
			goto end;
		} else {
			LOG_MSG(LOG__WARNING, false, "Queue semaphore succesfully adquired beore 5 seconds. Unlocking it.");
			if (sem_post(semaphore) != 0) {
				LOG_MSG(LOG__ERROR, true, "sem_post()");
				ret = 1;
				goto end;
			}
		}
	} else if (errno == EINVAL) {
		LOG_MSG(LOG__ERROR, true, "sem_trywait()");
		ret = 1;
		goto end;
	} else {
		if (sem_post(semaphore) != 0) {
			LOG_MSG(LOG__ERROR, true, "sem_post()");
			ret = 1;
			goto end;
		}
	}

end:

	return ret;
}

int try_rwlock(pthread_rwlock_t *rwlock)
{
	int ret = 0;
	struct timespec wait_time;
	wait_time.tv_nsec = 0;

	if (((errno = pthread_rwlock_trywrlock(rwlock)) != 0) && (errno == EBUSY || errno == EDEADLK)) {
		LOG_MSG(LOG__WARNING, true, "Read/write lock locked. Uncontrolled exit?. Waiting 5 seconds before resetting it.");
		wait_time.tv_sec = time(NULL) + 5;
		if (((errno = pthread_rwlock_timedwrlock(rwlock, &wait_time)) != 0) && (errno == ETIMEDOUT || errno == EDEADLK)) {
			LOG_MSG(LOG__WARNING, false, "Can't get read/write lock. Resetting it");
			if (pthread_rwlock_destroy(rwlock) != 0) {
				LOG_MSG(LOG__ERROR, true, "pthread_rwlock_destroy()");
				ret = 1;
				goto end;
			}
			if (pthread_rwlock_init(rwlock, NULL) != 0) {
				LOG_MSG(LOG__ERROR, true, "pthread_rwlock_init()");
				ret = 1;
				goto end;
			}
		} else if (errno == EINVAL) {
			LOG_MSG(LOG__ERROR, true, "pthread_rwlock_timedwrlock()");
			ret = 1;
			goto end;
		} else {
			LOG_MSG(LOG__WARNING, false, "Read/write lock succesfully adquired before 5 seconds. Unlocking it.");
			if (pthread_rwlock_unlock(rwlock) != 0) {
				LOG_MSG(LOG__ERROR, true, "pthread_rwlock_unlock()");
				ret = 1;
				goto end;
			}
		}
	} else if (errno == EINVAL) {
		LOG_MSG(LOG__ERROR, true, "pthread_rwlock_tryrwlock()");
		ret = 1;
		goto end;
	} else {
		if (pthread_rwlock_unlock(rwlock) != 0) {
			LOG_MSG(LOG__ERROR, true, "pthread_rwlock_unlock()");
			ret = 1;
			goto end;
		}
	}

end:

	return ret;
}

int load_global_config(adtn_ini_t **adtn_ini, const char *config_file)
{
	int ret = 0;
	char *tmp_value;
	struct conf_list global_configuration = {0};

	if (config_file == NULL) {
		ret = 1;
		goto end;
	}

	(*adtn_ini) = (adtn_ini_t*) calloc(1, sizeof(adtn_ini_t));
	strncpy((*adtn_ini)->ini_path, config_file, PATH_MAX);

	//Global options
	if (load_config("global", &global_configuration, config_file) != 1) {
		LOG_MSG(LOG__ERROR, false, "Error loading global config. Configuration file not found.");
		ret = 1;
		goto end;
	}

	if ((tmp_value = get_option_value("id", &global_configuration)) == NULL) {
		LOG_MSG(LOG__ERROR, false, "No ID (id) defined in the configuration file.");
		ret = 1;
		goto free1;
	}
	strncpy((*adtn_ini)->platform_id, tmp_value, MAX_PLATFORM_ID_LEN);

	if ((tmp_value = get_option_value("ip", &global_configuration)) == NULL) {
		LOG_MSG(LOG__ERROR, false, "No IP (ip) defined in the configuration file.");
		ret = 1;
		goto free1;
	}
	strncpy((*adtn_ini)->platform_ip, tmp_value, STRING_IP_LEN);

	if ((tmp_value = get_option_value("port", &global_configuration)) == NULL) {
		LOG_MSG(LOG__ERROR, false, "No platform port (port) defined in the configuration file.");
		ret = 1;
		goto free1;
	}
	(*adtn_ini)->platform_port = atoi(tmp_value);

	if ((tmp_value = get_option_value("data", &global_configuration)) == NULL) {
		LOG_MSG(LOG__ERROR, false, "No data dir (data) defined in the configuration file.");
		ret = 1;
		goto free1;
	}
	strncpy((*adtn_ini)->data_path, tmp_value, PATH_MAX);
	/**/

	//Log options
	if (load_config("log", &global_configuration, config_file) != 1) {
		LOG_MSG(LOG__ERROR, false, "Error loading global config. Configuration file not found.");
		ret = 1;
		goto end;
	}

	if ((tmp_value = get_option_value("log_file", &global_configuration)) == NULL) {
		LOG_MSG(LOG__ERROR, false, "No log file (log_file) defined in the configuration file.");
		ret = 1;
		goto free1;
	}
	if (atoi(tmp_value))
		(*adtn_ini)->log_file = true;
	else
		(*adtn_ini)->log_file = false;

	if ((tmp_value = get_option_value("log_level", &global_configuration)) == NULL) {
		LOG_MSG(LOG__ERROR, false, "No log level (log_level) defined in the configuration file.");
		ret = 1;
		goto free1;
	}
	(*adtn_ini)->log_level = atoi(tmp_value);
	/**/

free1:
	free_options_list(&global_configuration);
end:

	return ret;
}

int init_locks(struct common *shm)
{
	int ret = 1;

	if (!shm)
		goto end;

	if (pthread_rwlock_init(&shm->rwlock, NULL) != 0) {
		LOG_MSG(LOG__ERROR, true, "pthread_rwlock_unlock()");
		goto end;
	}

	ret = 0;
end:

	return ret;
}

int test_locks(struct common *shm)
{
	int ret = 1;
	if (!shm)
		goto end;

	if (try_rwlock(&shm->rwlock) != 0)
		goto end;

	ret = 0;
end:

	return ret;
}

int initilize_shm(adtn_ini_t *adtn_ini, struct common **shm_common, short int force_init)
{
	int ret = 0;
	unsigned int shm_suffix = 0;

	shm_suffix = Crc32_ComputeBuf(shm_suffix, adtn_ini->data_path, strlen(adtn_ini->data_path));

	if ((shm_fd = init_shared_memory(shm_suffix, true)) < 0) {
		LOG_MSG(LOG__ERROR, true, "Can't init shared memory: ");
		ret = 1;
		goto end;
	}

	*shm_common = NULL;
	if ((*shm_common = map_shared_memory(shm_fd, true)) == NULL) {
		LOG_MSG(LOG__ERROR, true, "Can't map shared memory: ");
		ret = 1;
		goto end;
	}

	// Lock shm file descriptor
	if (flock(shm_fd, LOCK_EX) != 0) {
		LOG_MSG(LOG__ERROR, true, "flock");
		ret = 1;
		goto end;
	}

	if (!(*shm_common)->initialized || force_init) {
		if ((ret = init_locks(*shm_common)) != 0)
			LOG_MSG(LOG__ERROR, false, "Error initializing shm locks");

		snprintf((*shm_common)->config_file, sizeof((*shm_common)->config_file), "%s", adtn_ini->ini_path);
		snprintf((*shm_common)->platform_id, sizeof((*shm_common)->platform_id), "%s", adtn_ini->platform_id);
		snprintf((*shm_common)->iface_ip, sizeof((*shm_common)->iface_ip), "%s", adtn_ini->platform_ip);
		snprintf((*shm_common)->data_path, sizeof((*shm_common)->data_path), "%s", adtn_ini->data_path);
		(*shm_common)->platform_port = adtn_ini->platform_port;
		(*shm_common)->prefix_id = shm_suffix;
		if (ret == 0)
			(*shm_common)->initialized = 1;
	} else if ((*shm_common)->initialized) {
		if (test_locks(*shm_common) != 0)
			LOG_MSG(LOG__ERROR, false, "Error resetting locks");
	}

	// Unlock shm file descriptor
	if (flock(shm_fd, LOCK_UN) != 0) {
		LOG_MSG(LOG__ERROR, true, "flock");
		ret = 1;
		goto end;
	}

end:
	return ret;
}

int init_adtn_process(int argc, char *const argv[], struct common **shm_common)
{
	int ret = 0;

	//Parse args
	char *config_file = NULL, absolute_config_file_path[PATH_MAX];
	short int force_init = 0;
	char ritPath[255] = {0};
	adtn_ini_t *adtn_ini = NULL;

	if (parse_adtn_options(argc, argv, &config_file, &force_init) != 0) {
		LOG_MSG(LOG__ERROR, false, "Error parsing command line options");
		ret = 1;
		goto end;
	}
	if (config_file == NULL)
		config_file = strdup(DEFAULT_CONF_FILE_PATH);

	//Check if file exists
	if (access(config_file, R_OK) != 0) {
		LOG_MSG(LOG__ERROR, false, "Configuration file not found.");
		ret = 1;
		goto free1;
	}

	if (realpath(config_file, absolute_config_file_path) == NULL) {
		LOG_MSG(LOG__ERROR, true, "realpath()");
		ret = 1;
		goto free1;
	}

	if (load_global_config(&adtn_ini, absolute_config_file_path) != 0) {
		LOG_MSG(LOG__ERROR, false, "Error loading config ");
		ret = 1;
		goto free1;
	}

	//Initialize log system according to user preferences
	set_debug_lvl(adtn_ini->log_level);
	set_log_file(adtn_ini->log_file);
	init_log(adtn_ini->data_path);

	//Set rit path
	snprintf(ritPath, 254, "%s/RIT", adtn_ini->data_path);
	rit_changePath(ritPath);

	//Initialize shared memory
	if (initilize_shm(adtn_ini, shm_common, force_init) != 0) {
		ret = -1;
		goto free1;
	}

free1:
	free(config_file);
	if (adtn_ini) 
		free(adtn_ini);
end:

	return ret;
}

int exit_adtn_process()
{
	struct common *shm = NULL;
	char data_path[255] = {0};

	end_log();

	if (shm_fd <= 0)
		goto end;
	if ((shm = map_shared_memory(shm_fd, true)) == NULL)
		goto close;
	strcpy(data_path, shm->data_path);

	if (unmap_shared_memory(shm) != 0)
		goto close;
	if (unlink(data_path) != 0)
		goto close;

close:
	close(shm_fd);
end:

	return 0;
}
