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

#ifndef INC_COMMON_CONFIG_H
#define INC_COMMON_CONFIG_H

#include "paths.h"

#include <pthread.h>
#include "shm.h"

struct conf_option {
	char *key;
	char *value;
	struct conf_option *next;
};

struct conf_list {
	struct conf_option *list;
	char *section;
	pthread_rwlock_t lock;
};

//Always required parameters to be loaded from adtn.ini config file
typedef struct adtn_ini {
	char ini_path[PATH_MAX];
	char data_path[PATH_MAX];
	char platform_id[MAX_PLATFORM_ID_LEN];
	char platform_ip[STRING_IP_LEN];
	short int platform_port;
	short int log_level;
	bool log_file;
} adtn_ini_t;

int load_config(const char *section, struct conf_list *config, const char *conf_file);
char *get_config(const char *key, struct conf_list *config);
int set_config(const char *key, const char *value, struct conf_list *config);

char *get_option_value(const char *key, struct conf_list *config);
char *get_option_value_r(const char *key, struct conf_list *config);
int set_option_value(const char *key, const char *value, struct conf_list *config);

int free_options_list(struct conf_list *config);

#endif
