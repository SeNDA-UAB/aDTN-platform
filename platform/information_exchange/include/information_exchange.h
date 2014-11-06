#ifndef INC_RITD_H
#define INC_RITD_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>     /* for address structs */
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <openssl/md5.h>
#include <regex.h>

#include "common/include/utlist.h"
#include "common/include/uthash.h"
#include "common/include/constants.h"
#include "common/include/rit.h"
#include "common/include/neighbours.h"
#include "common/include/stats.h"
#include "common/include/config.h"
#include "common/include/log.h"
#include "common/include/init.h"

#define READ_LOCK(lock) do { if (pthread_rwlock_rdlock(lock) != 0) err_msg(true, "pthread_rwlock_rdlock()"); } while (0)
#define WRITE_LOCK(lock) do { if (pthread_rwlock_wrlock(lock) != 0) err_msg(true, "pthread_rwlock_wrlock()"); } while (0)
#define UNLOCK_LOCK(lock) do { if (pthread_rwlock_unlock(lock) != 0) err_msg(true, "pthread_rwlock_unlock()"); } while (0)

#include "constants.h"
#include "hash.h"
#include "packet.h"
#include "packet_processor.h"
#include "debug.h"
#include "global.h"

struct thread_vars {
	//struct world_vars *world;
	unsigned char *recv_buffer;
	int recv_buffer_l;
	char src_ip[16];
	short int src_port;
};

#endif
