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

#ifndef INC_GLOBAL_H
#define INC_GLOBAL_H

// enum vars {
//         SET,
//         GET
// };

// enum working_modes {
//         COMPLETE,
//         SELECTIVE
// };

struct rit_info {
	bool update_beacon;
	//unsigned char actual_hash[MD5_DIGEST_LENGTH+1];
};

struct world_vars {
	int sender_sock;
	struct nbs_list nbs;
	struct common *shm;

	struct rit_info rit;

	//Config options
	char platform_ip[STRING_IP_LEN];
	char platform_id[MAX_PLATFORM_ID_LEN];
	int platform_port;
	char multicast_group[16];
	int multicast_port;
	short int announcement_period;
	bool announceable;
	int nbs_life;

	//Stats
	float last_ten_ewma;
	float last_ten_ewma_factor;
	float last_hundred_ewma;
	float last_hundred_ewma_factor;
	int num_contacts;

	bool test_mode;
	bool debug;

	pthread_rwlock_t rwlock;
};

int init_world();
int load_ritd_config(const char *config_file);
int free_world();

#endif