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

int load_config(const char *section, struct conf_list *config, const char *conf_file);
char *get_config(const char *key, struct conf_list *config);
int set_config(const char *key, const char *value, struct conf_list *config);

char *get_option_value(const char *key, struct conf_list *config);
char *get_option_value_r(const char *key, struct conf_list *config);
int set_option_value(const char *key, const char *value, struct conf_list *config);

int free_options_list(struct conf_list *config);

#endif
