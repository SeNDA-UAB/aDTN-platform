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

#include "include/config.h"
#include "include/utlist.h"
#include "include/log.h"
#include "include/minIni.h"

void load_option(const char *section, const char *key, const char *value, struct conf_list *config)
{
	if (strncmp(config->section, section, strlen(config->section)) == 0) {
		struct conf_option *option = (struct conf_option *)malloc(sizeof(struct conf_option));

		option->key = strdup(key);
		option->value = strdup(value);
		LL_APPEND(config->list, option);
	}
}

int load_config(const char *section, struct conf_list *config, const char *conf_file)
{
	config->section = strdup(section);
	return  ini_browse((INI_CALLBACK)load_option, config, conf_file);
}

int keycmp(struct conf_option *a, struct conf_option *b)
{
	return strcmp(a->key, b->key);
}

char *get_option_value(const char *key, struct conf_list *config)
{
	struct conf_option *elt = NULL;
	struct conf_option tmp;

	if (key == NULL || strcmp(key, "") == 0 || config == NULL)
		return NULL;
	tmp.key = (char *)key; // LL_SEARCH macro does not allow to cast to (char *)
	LL_SEARCH(config->list, elt, &tmp, keycmp);

	if (elt != NULL)
		return elt->value;
	else
		return NULL;
}

char *get_option_value_r(const char *key, struct conf_list *config)
{
	char *option = NULL;

	if (pthread_rwlock_rdlock(&config->lock) != 0)
		LOG_MSG(LOG__ERROR, errno, "pthread_rwlock_rdlock()");

	option = get_option_value(key, config);

	if (pthread_rwlock_unlock(&config->lock) != 0)
		LOG_MSG(LOG__ERROR, errno, "pthread_unlock_rdlock()");

	return option;
}

int set_option_value(const char *key, const char *value, struct conf_list *config)
{
	struct conf_option *option = NULL;
	struct conf_option tmp;

	if (key == NULL || value == NULL || config == NULL || strcmp(key, "") == 0 || strcmp(value, "") == 0)
		return 1;

	tmp.key = strdup(key);
	LL_SEARCH(config->list, option, &tmp, keycmp);
	free(tmp.key);

	if (option != NULL) {
		if (option->value != NULL) {
			free(option->value);
			option->value = strdup(value);
		} else {
			option->value = strdup(value);
		}
	} else {
		option = (struct conf_option *)malloc(sizeof(struct conf_option));
		option->key = strdup(key);
		option->value = strdup(value);

		LL_APPEND(config->list, option);
	}

	return 0;
}

int set_option_value_r(const char *key, const char *value, struct conf_list *config)
{
	int result = 1;

	if (pthread_rwlock_wrlock(&config->lock) != 0)
		LOG_MSG(LOG__ERROR, errno, "pthread_rwlock_wrlock()");

	result = set_option_value(key, value, config);

	if (pthread_rwlock_unlock(&config->lock) != 0)
		LOG_MSG(LOG__ERROR, errno, "pthread_rwlock_unlock()");

	return result;
}


int free_options_list(struct conf_list *config)
{
	struct conf_option *option, *tmp;
	LL_FOREACH_SAFE(config->list, option, tmp) {
		if (option->key != NULL) free(option->key);
		if (option->value != NULL) free(option->value);
		LL_DELETE(config->list, option);
		free(option);
	}
	free(config->section);

	return 0;
}