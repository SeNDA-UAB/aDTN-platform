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
	        "       -c [config_file]      %s will use the specified config file instead of the default found at "DEFAULT_CONF_FILE"\n"
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
		log_message(LOG_WARNING, "Queue semaphore locked. Uncontrolled exit?. Waiting 5 seconds before resetting it.", errno);
		wait_time.tv_sec = time(NULL) + 5;
		if (sem_timedwait(semaphore, &wait_time) != 0 && (errno == ETIMEDOUT || errno == EDEADLK)) {
			log_message(LOG_WARNING, "Can't get queue semaphore. Resetting it", 0);
			if (sem_destroy(semaphore) != 0) {
				log_message(LOG_WARNING, "sem_destroy()", errno);
				ret = 1;
				goto end;
			}
			if (sem_init(semaphore, 1, 1) != 0) {
				log_message(LOG_WARNING, "sem_init()", errno);
				ret = 1;
				goto end;
			}
		} else if (errno == EINVAL) {
			log_message(LOG_WARNING, "sem_timedwait()", errno);
			ret = 1;
			goto end;
		} else {
			log_message(LOG_WARNING, "Queue semaphore succesfully adquired beore 5 seconds. Unlocking it.", 0);
			if (sem_post(semaphore) != 0) {
				log_message(LOG_WARNING, "sem_post()", errno);
				ret = 1;
				goto end;
			}
		}
	} else if (errno == EINVAL) {
		log_message(LOG_WARNING, "sem_trywait()", errno);
		ret = 1;
		goto end;
	} else {
		if (sem_post(semaphore) != 0) {
			log_message(LOG_WARNING, "sem_post()", errno);
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
		log_message(LOG_WARNING, "Read/write lock locked. Uncontrolled exit?. Waiting 5 seconds before resetting it.", errno);
		wait_time.tv_sec = time(NULL) + 5;
		if (((errno = pthread_rwlock_timedwrlock(rwlock, &wait_time)) != 0) && (errno == ETIMEDOUT || errno == EDEADLK)) {
			log_message(LOG_WARNING, "Can't get read/write lock. Resetting it", 0);
			if (pthread_rwlock_destroy(rwlock) != 0) {
				log_message(LOG_WARNING, "pthread_rwlock_destroy()", errno);
				ret = 1;
				goto end;
			}
			if (pthread_rwlock_init(rwlock, NULL) != 0) {
				log_message(LOG_WARNING, "pthread_rwlock_init()", errno);
				ret = 1;
				goto end;
			}
		} else if (errno == EINVAL) {
			log_message(LOG_WARNING, "pthread_rwlock_timedwrlock()", errno);
			ret = 1;
			goto end;
		} else {
			log_message(LOG_WARNING, "Read/write lock succesfully adquired before 5 seconds. Unlocking it.", 0);
			if (pthread_rwlock_unlock(rwlock) != 0) {
				log_message(LOG_WARNING, "pthread_rwlock_unlock()", errno);
				ret = 1;
				goto end;
			}
		}
	} else if (errno == EINVAL) {
		log_message(LOG_WARNING, "pthread_rwlock_tryrwlock()", errno);
		ret = 1;
		goto end;
	} else {
		if (pthread_rwlock_unlock(rwlock) != 0) {
			log_message(LOG_WARNING, "pthread_rwlock_unlock()", errno);
			ret = 1;
			goto end;
		}
	}

end:

	return ret;
}

int load_global_config(struct common *shm, const char *config_file)
{
	int ret = 0;
	struct conf_list global_configuration = {0};

	if (shm == NULL || config_file == NULL) {
		ret = 1;
		goto end;
	}

	if (load_config("global", &global_configuration, config_file) != 1) {
		log_message(LOG_WARNING, "Error loading global config. Configuration file not found.", 0);
		ret = 1;
		goto end;
	}

	//Required options
	if (get_option_value("ip", &global_configuration) == NULL) {
		log_message(LOG_WARNING, "No IP (ip) defined in the configuration file.", 0);
		ret = 1;
		goto free1;
	}

	if (get_option_value("id", &global_configuration) == NULL) {
		log_message(LOG_WARNING, "No IP (ip) defined in the configuration file.", 0);
		ret = 1;
		goto free1;
	}

	if (get_option_value("port", &global_configuration) == NULL) {
		log_message(LOG_WARNING, "No platform port (port) defined in the configuration file.", 0);
		ret = 1;
		goto free1;
	}

	if (get_option_value("data", &global_configuration) == NULL) {
		log_message(LOG_WARNING, "No data dir (data) defined in the configuration file.", 0);
		ret = 1;
		goto free1;
	}

	if (pthread_rwlock_wrlock(&shm->rwlock) != 0) {
		log_message(LOG_WARNING, "pthread_rwlock_wrlock()", errno);
		goto free1;
	}

	strncpy(shm->iface_ip, get_option_value("ip", &global_configuration), STRING_IP_LEN);
	strncpy(shm->platform_id, get_option_value("id", &global_configuration), MAX_PLATFORM_ID_LEN);
	shm->platform_port = strtol(get_option_value("port", &global_configuration), NULL, 10);
	strncpy(shm->data_path, get_option_value("data", &global_configuration), 255);

	/**/
	if (pthread_rwlock_unlock(&shm->rwlock) != 0) {
		log_message(LOG_WARNING, "pthread_rwlock_unlock", errno);
	}

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
		err_msg(true, "pthread_rwlock_unlock()");
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

	if (try_rwlock(&shm->rwlock) != 0) {
		goto end;
	}

	/**/
	ret = 0;
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
	char data_path[255] = {0};
	unsigned int shm_suffix = 0;

	if (parse_adtn_options(argc, argv, &config_file, &force_init) != 0) {
		log_message(LOG_WARNING, "Error parsing command line options", 0);
		ret = 1;
		goto end;
	}
	if (config_file == NULL)
		config_file = strdup(DEFAULT_CONF_FILE);

	//Check if file exists
	if (access(config_file, R_OK) != 0) {
		log_message(LOG_WARNING, "Configuration file not found.", 0);
		ret = 1;
		goto free1;
	}

	//Get data path
	if (ini_gets("global", "data", "", data_path, 255, config_file) == 0) {
		log_message(LOG_WARNING, "Data path not found in the configuration file", 0);
		ret = 1;
		goto free1;
	}

	//Set rit path
	snprintf(ritPath, 254, "%s/RIT", data_path);
	rit_changePath(ritPath);

	/*SHM*/
	shm_suffix = Crc32_ComputeBuf(shm_suffix, data_path, strlen(data_path));

	if ((shm_fd = init_shared_memory(shm_suffix, true)) < 0) {
		log_message(LOG_WARNING, "Can't init shared memory: ", errno);
		ret = -1;
		goto free1;
	}
	*shm_common = NULL;
	if ((*shm_common = map_shared_memory(shm_fd, true)) == NULL) {
		log_message(LOG_WARNING, "Can't map shared memory: ", errno);
		ret = -1;
		goto free1;
	}

	// Lock shm fd
	if (flock(shm_fd, LOCK_EX) != 0) {
		err_msg(true, "flock");
		ret = -1;
		goto free1;
	}

	if (!(*shm_common)->initialized || force_init) {
		if ((ret = init_locks(*shm_common)) != 0)
			err_msg(false, "Error initializing shm locks");

		if (realpath(config_file, absolute_config_file_path) == NULL)
			err_msg(true, "realpath()");

		snprintf((*shm_common)->config_file, 255, "%s", absolute_config_file_path);
		if ((ret = load_global_config(*shm_common, absolute_config_file_path)) != 0)
			err_msg(false, "Error loading config into shm: ");

		(*shm_common)->prefix_id = shm_suffix;

		if (ret == 0)
			(*shm_common)->initialized = 1;
	} else if ((*shm_common)->initialized) {
		if (test_locks(*shm_common) != 0)
			err_msg(false, "Error resetting locks");
	}

	// Unlock shm fd
	if (flock(shm_fd, LOCK_UN) != 0) {
		err_msg(true, "flock");
		ret = -1;
		goto free1;
	}

	/**/
	pthread_rwlock_rdlock(&(*shm_common)->rwlock);
	init_log((*shm_common)->data_path);
	pthread_rwlock_unlock(&(*shm_common)->rwlock);

free1:
	free(config_file);
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
