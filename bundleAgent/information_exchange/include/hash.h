#ifndef INC_HASH_H
#define INC_HASH_H

/*Neighbours structures*/

struct nb_info {
	char id[MAX_PLATFORM_ID_LEN];
	char ip[STRING_IP_PORT_LEN];
	int port;
	long int first_seen;
	long int last_seen;
	pthread_rwlock_t rwlock;
	UT_hash_handle hh;
};

struct nbs_list {
	struct nb_info *list;
	unsigned short int num_nbs;
	pthread_rwlock_t rwlock;
};


#endif