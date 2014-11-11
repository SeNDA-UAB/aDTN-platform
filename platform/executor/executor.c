#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <limits.h> // NAME_MAX (max filename lenght)
#include <stdint.h> //
#include <stddef.h>
#include <netinet/in.h>
#include <sched.h>
#include <dlfcn.h>

#include "common/include/init.h"
#include "common/include/common.h"

#include "modules/include/hash.h"
#include "modules/include/exec.h"

#include "include/world.h"
#include "include/executor.h"
#include "worker.c"

/* Cleaning */
int clean_bundle_dl(bundle_code_dl_s *b_dl)
{
	if (b_dl->info.dest != NULL)
		free(b_dl->info.dest);
	if (b_dl->dls.routing != NULL) {
		b_dl->dls.routing->refs--;
		if (b_dl->dls.routing->refs == 0) { // Also unload and remove loaded code
			routing_code_dl_remove(b_dl->dls.routing);
			free((char *)b_dl->dls.routing->code);
			dlclose(b_dl->dls.routing->dl->handler);
			free(b_dl->dls.routing->dl);
			free(b_dl->dls.routing);
		}
	}
	if (b_dl->dls.life != NULL) {
		b_dl->dls.life->refs--;
		if (b_dl->dls.life->refs == 0) {
			life_code_dl_remove(b_dl->dls.life);
			free((char *)b_dl->dls.life->code);
			dlclose(b_dl->dls.life->dl->handler);
			free(b_dl->dls.life->dl);
			free(b_dl->dls.life);
		}
	}
	if (b_dl->dls.prio != NULL) {
		b_dl->dls.prio->refs--;
		if (b_dl->dls.prio->refs == 0) {
			prio_code_dl_remove(b_dl->dls.prio);
			free((char *)b_dl->dls.prio->code);
			dlclose(b_dl->dls.prio->dl->handler);
			free(b_dl->dls.prio->dl);
			free(b_dl->dls.prio);
		}
	}
	free((char *)b_dl->bundle_id);

	return 0;
}

int clean_all_bundle_dl(void)
{
	return del_map_bundle_dl_table(clean_bundle_dl);
}

/**/
ssize_t initialize_socket(char *sockname)
{
	ssize_t ret = 0;
	int s = 0;
	struct sockaddr_un addr = {0};
	struct stat stat_file = {0};

	if ((s = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
		LOG_MSG(LOG__ERROR, true, "socket()");
		ret = -1;
		goto end;
	}

	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sockname, sizeof(addr.sun_path));

	//If socket exists, delete it
	if (stat(sockname, &stat_file) == 0) {
		unlink(sockname);
	}

	if ((ret = bind(s, (struct sockaddr *) &addr, sizeof(struct sockaddr_un))) == -1) {
		LOG_MSG(LOG__ERROR, true, "bind()");
		goto end;
	}
end:
	if (ret == -1) {
		close(s);
		unlink(sockname);
	} else {
		ret = s;
	}

	return ret;
}

int main(int argc, char *const argv[])
{
	int ret = 0;
	int main_socket = 0, sig = 0;
	unsigned short i = 0;
	size_t bundles_path_l = 0, sockname_l = 0, objects_path_l = 0;
	char *sockname  = NULL;
	uint8_t *respawn_child = NULL;
	sigset_t blocked_sigs = {{0}};
	siginfo_t si = {0};
	struct stat folder_stat = {0};

	pthread_mutex_t **respawn_child_mutex = NULL;
	pthread_mutex_t *preparing_exec = NULL;
	pthread_t threads_pool[POOL_SIZE] = {0};

	/*
	 Block singals so initialization is not interrumpted
	 and to handle all signals from main process
	 (new threads are created with the signals in blocked_sigs masked)
	*/
	if (sigaddset(&blocked_sigs, SIGINT) != 0)
		LOG_MSG(LOG__ERROR, true, "sigaddset()");
	if (sigaddset(&blocked_sigs, SIGTERM) != 0)
		LOG_MSG(LOG__ERROR, true, "sigaddset()");
	if (sigprocmask(SIG_BLOCK, &blocked_sigs, NULL) == -1)
		LOG_MSG(LOG__ERROR, true, "sigprocmask()");

	//Init aDTN process (shm and global option)
	if (init_adtn_process(argc, argv, &world.shm) != 0) {
		LOG_MSG(LOG__ERROR, false, "Process initialization failed. Aborting execution");
		ret = 1;
		goto end;
	}

	pthread_rwlock_rdlock(&world.shm->rwlock);
	// Prepare global functions
	if (world.shm->data_path == NULL || *world.shm->data_path == '\0') {
		LOG_MSG(LOG__ERROR, false, "Path of data folder not initialized.");
		ret = 1;
		goto end;

	}
	bundles_path_l = strlen(world.shm->data_path) + strlen(SPOOL_PATH) + 1;
	world.bundles_path = (char *)malloc(bundles_path_l);
	snprintf(world.bundles_path, bundles_path_l, "%s%s", world.shm->data_path, SPOOL_PATH);

	if (stat(world.bundles_path, &folder_stat) != 0) {
		LOG_MSG(LOG__ERROR, true, "Can't find bundles folder %s. Creating it.", world.bundles_path);
		if (mkdir(world.bundles_path, 0755) != 0 && errno !=  EEXIST) {
			LOG_MSG(LOG__ERROR, true, "Error creating objects path %s", world.bundles_path);
			ret = 1;
			goto end;
		}
	}

	// Initialize global socket
	if (sizeof(world.shm->prefix_id) > 4) {
		LOG_MSG(LOG__ERROR, false, "Platform hash (prefix_id) too long");
		ret = 1;
		goto end;
	}
	//sockname format <DEF_SOCKNAME>-<platform_hash>.sock platform_hash max. lenght 8 chars.
	sockname_l = strlen(world.shm->data_path) + strlen(DEF_SOCKNAME) + 7;
	sockname = malloc(sockname_l);
	snprintf(sockname, sockname_l, "%s/%s.sock", world.shm->data_path, DEF_SOCKNAME);

	main_socket = initialize_socket(sockname);
	if (main_socket == -1) {
		LOG_MSG(LOG__ERROR, true, "Socket initialization failed");
		ret = 1;
		goto end;
	}

	// Create paths
	objects_path_l = strlen(world.shm->data_path) + strlen(OBJECTS_PATH) + 2;
	world.objects_path = (char *)malloc(objects_path_l);
	snprintf(world.objects_path, objects_path_l, "%s/%s", world.shm->data_path, OBJECTS_PATH);
	if (mkdir(world.objects_path, 0755) != 0 && errno !=  EEXIST) {
		LOG_MSG(LOG__ERROR, true, "Error creating objects path %s", world.objects_path);
		ret = 1;
		goto end;
	}
	pthread_rwlock_unlock(&world.shm->rwlock);

	//Init threads pool
	respawn_child = (uint8_t *)calloc(1, sizeof(uint8_t) * POOL_SIZE);
	respawn_child_mutex = (pthread_mutex_t **)calloc(POOL_SIZE, sizeof(pthread_mutex_t *));
	for ( i = 0; i <  POOL_SIZE; i++) {
		respawn_child_mutex[i] = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(respawn_child_mutex[i], NULL);
	}

	// Prepare common lock
	preparing_exec = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(preparing_exec, NULL);

	for (i = 0; i < POOL_SIZE; i++) {
		// Prepare params
		worker_params *params = (worker_params *)malloc(sizeof(worker_params));
		params->thread_num = i;
		params->main_socket = main_socket;
		params->preparing_exec = preparing_exec;
		params->respawn_child = respawn_child;
		params->respawn_child_mutex = respawn_child_mutex;

		// Create threads
		if (pthread_create(&threads_pool[i], NULL, (void *) worker_thread, (void *) params) != 0) {
			LOG_MSG(LOG__ERROR, true, "Can't init pool of workers");
			ret = 1;
			goto end;
		}
	}

	//Wait for signals
	for (;;) {
		sig = sigwaitinfo(&blocked_sigs, &si);

		if (sig == SIGINT || sig == SIGTERM)
			break;
	}

	//Exit gracefully
	for (i = 0; i < POOL_SIZE; i++) {
		if (pthread_cancel(threads_pool[i]) != 0) {
			LOG_MSG(LOG__ERROR, true, "Can't cancel worker thread %d", i);
		}
	}

	//Wait for all threads to finish
	for (i = 0; i < POOL_SIZE; i++) {
		if (pthread_join(threads_pool[i], NULL) != 0) {
			LOG_MSG(LOG__ERROR, true, "Can't join worker thread %d", i);
		}
	}


end:
	if (close(main_socket) == -1)
		LOG_MSG(LOG__ERROR, true, "close()");
	if (unlink(sockname) == -1)
		LOG_MSG(LOG__ERROR, true, "unlink()");
	if (sockname)
		free(sockname);
	if (preparing_exec)
		free(preparing_exec);
	if (world.bundles_path)
		free(world.bundles_path);
	if (respawn_child)
		free(respawn_child);

	if (respawn_child_mutex) {
		for ( i = 0; i <  POOL_SIZE; i++) {
			if (respawn_child_mutex[i])
				free(respawn_child_mutex[i]);
		}
		free(respawn_child_mutex);
	}

	clean_all_bundle_dl();
	exit_adtn_process();

	return ret;
}