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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include "constants.h"
#include "cJSON.h"
#include "rit.h"
#include "config.h"
#include "log.h"


typedef struct list {
	char *value;
	struct list *next;
} list_s;

static char rit_path[100] = {0};
static cJSON *all = NULL;
static FILE *rit_fd;
static pthread_mutex_t ritM = PTHREAD_MUTEX_INITIALIZER;

/* Utility funtcion*/
int rit_lock()
{
	struct flock f1;
	f1.l_type = F_WRLCK;
	f1.l_whence = SEEK_SET;
	f1.l_start = 0;
	f1.l_len = 0;
	f1.l_pid = getpid();

	if (fcntl(fileno(rit_fd), F_SETLKW, &f1) == -1) {
		LOG_MSG(LOG__ERROR, true, "Can't lock RIT");
		return 1;
	}

	return 0;
}

int rit_unlock()
{
	struct flock f1;
	f1.l_type = F_UNLCK;
	f1.l_whence = SEEK_SET;
	f1.l_start = 0;
	f1.l_len = 0;
	f1.l_pid = getpid();

	if (fcntl(fileno(rit_fd), F_SETLK, &f1) == -1) {
		LOG_MSG(LOG__ERROR, true, "Can't unlock RIT");
		return 1;
	}

	return 0;
}

/* Platform functions */
int rit_changePath(const char *newPath)
{
	FILE *f;

	if (!newPath) {
		memset(rit_path, 0, 100);
		if (*rit_path == '\0') {
			struct conf_list global_configuration = {0};
			if (load_config("global", &global_configuration, DEFAULT_CONF_FILE_PATH) != 1) {
				return 1;
			}
			char *data_path = get_option_value("data", &global_configuration);
			snprintf(rit_path, 99, "%s/RIT", data_path);

			free_options_list(&global_configuration);
		}
		return 0;
	}

	if (strcmp(newPath, "") == 0)
		return 1;

	if (strlen(newPath) > 99)
		return 1;

	f = fopen(newPath, "r");
	if (!f) {
		f = fopen(newPath, "a");
		if (!f) {
			return 1;
		}
	}
	fclose(f);
	memset(rit_path, 0, 100);
	strncpy(rit_path, newPath, 99);

	return 0;
}

/* List Functions*/
static list_s *parse(const char *string_to_parse, const char *delimiter)
{


	char *string_copy;
	list_s *out_list = NULL, *ptr_list;
	char *save_ptr, *token;
	int len;

	if ((string_to_parse == NULL) || (strcmp(string_to_parse, "") == 0))
		return NULL;

	string_copy = strdup(string_to_parse);
	out_list = (list_s *)malloc(sizeof(list_s));
	ptr_list = out_list;
	token = strtok_r(string_copy, delimiter, &save_ptr);
	if (token == NULL)
		return NULL;
	len = strlen(token) + 1;
	ptr_list->value = (char *)calloc(len, sizeof(char));
	strncpy(ptr_list->value, token, len);
	ptr_list->next = NULL;

	do {
		token = strtok_r(NULL, delimiter, &save_ptr);
		if (token == NULL)
			break;
		ptr_list->next = (list_s *)calloc(sizeof(list_s), 1);
		ptr_list = ptr_list->next;
		len = strlen(token) + 1;
		ptr_list->value = (char *)calloc(len, sizeof(char));
		strncpy(ptr_list->value, token, len);
		ptr_list->next = NULL;
	} while (token != NULL);

	if (string_copy) free(string_copy);
	return out_list;
}

static void clean_list(list_s *list_to_clean)
{
	list_s *ptr_list;

	while (list_to_clean != NULL) {
		if (list_to_clean->value)
			free(list_to_clean->value);
		ptr_list = list_to_clean;
		list_to_clean = list_to_clean->next;
		free(ptr_list);
	}
}

/*Helper functions*/
static cJSON *get_full_rit(int only_block)
{
	char *data;
	int len;
	int query_started = 0;
	int b_read;
	cJSON *fullJSON = NULL;

	if (*rit_path == '\0') {
		struct conf_list global_configuration = {0};
		if (load_config("global", &global_configuration, DEFAULT_CONF_FILE_PATH) != 1) {
			return NULL;
		}
		char *data_path = get_option_value("data", &global_configuration);
		snprintf(rit_path, 99, "%s/RIT", data_path);
		free_options_list(&global_configuration);
	}
	if (rit_fd != NULL) {
		query_started = 1;
	} else {
		rit_fd = fopen(rit_path, "r+b");
	}
	if (rit_fd == NULL)
		return NULL;
	if (only_block == 1) {
		rit_lock();
	}

	fseek(rit_fd, 0, SEEK_END);
	len = ftell(rit_fd);
	fseek(rit_fd, 0, SEEK_SET);

	data = (char *)calloc(len + 1, 1);
	b_read = fread(data, 1, len, rit_fd);

	if (only_block == 0) {
		rit_unlock();
		if (query_started == 0) {
			fclose(rit_fd);
			rit_fd = NULL;
		}
	}

	if (b_read == len) {
		fullJSON = cJSON_Parse(data);
		free(data);
	}

	return fullJSON;
}


static void save_JSON_to_rit(cJSON *element, int only_unblock)
{
	char *out;
	char tmp_path[105] = {0};
	FILE *f;

	if (*rit_path == '\0') {
		struct conf_list global_configuration = {0};
		char *data_path = get_option_value("data", &global_configuration);
		snprintf(rit_path, 99, "%s/RIT", data_path);

		free_options_list(&global_configuration);
	}

	snprintf(tmp_path, 105, "%s.tmp", rit_path);

	struct flock f1;
	f1.l_type = F_WRLCK;
	f1.l_whence = SEEK_SET;
	f1.l_start = 0;
	f1.l_len = 0;
	f1.l_pid = getpid();

	out = cJSON_Print(element);
	f = fopen(tmp_path, "w");
	fcntl(fileno(f), F_SETLKW, &f1);

	fprintf(f, "%s", out);

	f1.l_type = F_UNLCK;
	fcntl(fileno(f), F_SETLK, &f1);
	fclose(f);

	fclose(rit_fd);
	rit_fd = NULL;
	remove(rit_path);
	rename(tmp_path, rit_path);

	free(out);
}

static cJSON *get_element_by_path(const char *path)
{
	cJSON *fullJSON, *JSONelement, *return_value = NULL;
	list_s *split_path, *ptr_list = NULL;

	fullJSON = get_full_rit(0);

	if (!fullJSON)
		return NULL;

	if (strcmp(path, "/") == 0) {
		return fullJSON;
	}

	split_path = parse(path, SEPARATOR);
	ptr_list = split_path;
	int i = 0;
	while (ptr_list != NULL) {
		JSONelement = i ? cJSON_GetObjectItem(JSONelement, ptr_list->value) : cJSON_GetObjectItem(fullJSON, ptr_list->value);
		if (!JSONelement)
			break;
		ptr_list = ptr_list->next;
		++i;
	}
	return_value = JSONelement ? cJSON_Duplicate(JSONelement, 1) : NULL;
	clean_list(split_path);
	cJSON_Delete(fullJSON);

	return return_value;
}

static char *get_tag_names_from_JSON(cJSON *json)
{
	cJSON *final_value, *single_tag;
	int tags_number = 0;
	int off;
	int len;
	char *out_value = NULL;

	final_value = cJSON_GetObjectItem(json, "tags");
	tags_number = final_value ? cJSON_GetArraySize(final_value) : 0;
	while (tags_number > 0) {
		single_tag = cJSON_GetArrayItem(final_value, tags_number - 1);
		off = out_value ? strlen(out_value) : 0;
		len = off + strlen(single_tag->string) + 1 + 1;
		char *new_data = (char *)realloc(out_value, len);
		out_value = new_data ? new_data : out_value;
		if (off) {
			strncpy(out_value + off, ";", 1);
			++off;
		}
		strncpy(out_value + off, single_tag->string, len - off);

		--tags_number;
	}

	return out_value;
}

char *rit_search(cJSON *root, const char *name, const char *value, char **full_path)
{
	cJSON *match_item, *single_item, *tag_items;
	char *tag_names, *path = NULL, *other_paths = NULL;
	char *complete_path;
	int len, off, element_number;

	if (!root)
		return NULL;
	if (!root->string) root->string = strdup("");
	len = strlen(root->string) + 1;
	len = *full_path ? len + strlen(*full_path) + 1 : len;
	complete_path = (char *) calloc(len, sizeof(char));
	if (*full_path)
		snprintf(complete_path, len, "%s/%s", *full_path, root->string);
	else
		strncpy(complete_path, root->string, len);

	tag_names = get_tag_names_from_JSON(root);
	if ((tag_names != NULL) && (strstr(tag_names, name) != NULL)) {
		tag_items = cJSON_GetObjectItem(root, "tags");
		match_item = cJSON_GetObjectItem(tag_items, name);
		if (strcmp(match_item->valuestring, value) == 0) {
			path = strdup(complete_path);
		}
	}

	if (tag_names) free(tag_names);

	element_number = cJSON_GetArraySize(root);
	while (element_number > 0) {
		single_item = cJSON_GetArrayItem(root, element_number - 1);
		if (single_item->string != NULL) {
			if ((strcmp(single_item->string, "value") != 0) && (strcmp(single_item->string, "tags") != 0)) {
				other_paths = rit_search(single_item, name, value, &complete_path);
				off = path ? strlen(path) : 0;
				len = other_paths ? off + strlen(other_paths) + 2 : 0;
				if (len) {
					char *new_path = (char *)realloc(path, len);
					path = new_path ? new_path : path;
				}
				if (off && len) {
					strncpy(path + off, ";", 1);
					++off;
				}
				if (len)
					strncpy(path + off, other_paths, len - off);
				if (other_paths)
					free(other_paths);
			}
		}
		--element_number;
	}
	free(complete_path);

	return path;
}

int rit_start()
{
	cJSON *newNode, *tags;
	pthread_mutex_lock(&ritM);
	all = get_full_rit(1);
	if (!all) {
		all = cJSON_CreateObject();
		tags = cJSON_CreateObject();
		newNode = cJSON_CreateString("");
		cJSON_AddItemToObject(all, "tags", tags);
		cJSON_AddItemToObject(all, "value", newNode);
	}

	return 0;
}

int rit_commit()
{
	save_JSON_to_rit(all, 1);
	pthread_mutex_unlock(&ritM);
	cJSON_Delete(all);
	all = NULL;

	return 0;
}

int rit_rollback()
{
	int ret = 1;
	cJSON_Delete(all);
	all = NULL;
	rit_unlock();
	all = get_full_rit(0);
	if (!all) {
		LOG_MSG(LOG__ERROR, false, "Rollback fails");
		goto end;
	}
	save_JSON_to_rit(all, 0);
	pthread_mutex_unlock(&ritM);
	cJSON_Delete(all);
	all = NULL;

	ret = 0;
end:

	return ret;
}

/* Values */
char *rit_getValue(const char *path)
{

	cJSON *finalValue, *JSONelement;
	char *outValue = NULL;

	if (path == NULL || strcmp(path, "") == 0)
		goto ret;

	pthread_mutex_lock(&ritM);
	JSONelement = get_element_by_path(path);
	if (!JSONelement)
		goto end;
	finalValue = cJSON_GetObjectItem(JSONelement, "value");
	outValue = finalValue ? strdup(finalValue->valuestring) : NULL;
	cJSON_Delete(JSONelement);

end:
	pthread_mutex_unlock(&ritM);
ret:

	return outValue;
}

int rit_set_base(const char *path, const char *value)
{
	cJSON *son, *grandson, *newNode, *tags;
	list_s *splitPath, *ptrList = NULL;
	int retValue = 0;
	int i;

	if (path == NULL || strcmp(path, "") == 0) {
		LOG_MSG(LOG__ERROR, false, "Path must be a valid value");
		return 1;
	}
	if (!value) {
		LOG_MSG(LOG__ERROR, false, "set: Value is mandatory for function set");
		return 1;
	}

	if (!all) {
		LOG_MSG(LOG__ERROR, false, "unset: unable to retrieve RIT");
		goto end;
	}

	splitPath = parse(path, SEPARATOR);
	if (!splitPath) {
		cJSON_Delete(all);
		LOG_MSG(LOG__ERROR, false, "set: Invalid path");
		retValue = -1;
		goto end;
	}
	ptrList = splitPath;
	i = 0;
	while (ptrList != NULL) {
		if (i != 0) {
			if (cJSON_GetObjectItem(son, ptrList->value) == NULL) {
				grandson = cJSON_CreateObject();
				tags = cJSON_CreateObject();
				cJSON_AddItemToObject(grandson, "tags", tags);
				cJSON_AddItemToObject(son, ptrList->value, grandson);

			}
			++i;
			son = cJSON_GetObjectItem(son, ptrList->value);
		} else {
			if (cJSON_GetObjectItem(all, ptrList->value) == NULL) {
				grandson = cJSON_CreateObject();
				tags = cJSON_CreateObject();
				cJSON_AddItemToObject(grandson, "tags", tags);
				cJSON_AddItemToObject(all, ptrList->value, grandson);
			}
			++i;
			son = cJSON_GetObjectItem(all, ptrList->value);
		}
		ptrList = ptrList->next;
	}

	newNode = cJSON_GetObjectItem(son, "value");
	if (newNode)
		cJSON_DeleteItemFromObject(son, "value");
	grandson = cJSON_CreateString(value);
	cJSON_AddItemToObject(son, "value", grandson);

	clean_list(splitPath);
end:

	return retValue;
}

int rit_var_set(const char *path, const char *value, ...)
{
	int ret;
	int transaction;
	va_list arg_list;

	va_start(arg_list, value);
	transaction = va_arg(arg_list, int);
	va_end(arg_list);

	if (transaction == 1) {
		ret = rit_set_base(path, value);
	} else {
		if (rit_start() != 0) {
			ret = 1;
			goto end;
		}
		ret = rit_set_base(path, value);
		rit_commit();
	}
end:

	return ret;
}

int rit_unset_base(const char *path)
{
	cJSON *son, *grandson, *aux;
	list_s *splitPath, *ptrList = NULL;
	int i = 0, retValue = 1;

	if (path == NULL || strcmp(path, "") == 0)
		goto end;

	if (!all) {
		LOG_MSG(LOG__ERROR, false, "unset: unable to retrieve RIT");
		goto end;
	}

	splitPath = parse(path, SEPARATOR);
	ptrList = splitPath;
	while (ptrList != NULL) {
		son = i ? cJSON_GetObjectItem(son, ptrList->value) : cJSON_GetObjectItem(all, ptrList->value);
		if (!son)
			goto end;
		ptrList = ptrList->next;
		++i;
	}

	aux = cJSON_GetObjectItem(son, "value");
	if (aux != NULL)
		cJSON_DeleteItemFromObject(son, "value");
	grandson = cJSON_CreateString("");
	cJSON_AddItemToObject(son, "value", grandson);

	clean_list(splitPath);
	retValue = 0;
end:

	return retValue;
}

int rit_var_unset(const char *path, ...)
{
	int ret;
	int transaction;
	va_list arg_list;

	va_start(arg_list, path);
	transaction = va_arg(arg_list, int);
	va_end(arg_list);

	if (transaction == 1) {
		ret = rit_unset_base(path);
	} else {
		if (rit_start() != 0) {
			ret = 1;
			goto end;
		}
		ret = rit_unset_base(path);
		rit_commit();
	}
end:

	return ret;
}

/* Tags */
char *rit_getTag(const char *path, const char *nom)
{
	cJSON *finalValue, *JSONelement, *tagsElement;
	char *outValue = NULL;

	if (path == NULL || strcmp(path, "") == 0 || nom == NULL || strcmp(nom, "") == 0)
		goto ret;
	pthread_mutex_lock(&ritM);

	JSONelement = get_element_by_path(path);
	if (!JSONelement)
		goto end;
	tagsElement = cJSON_GetObjectItem(JSONelement, "tags");
	finalValue = tagsElement ? cJSON_GetObjectItem(tagsElement, nom) : NULL;
	outValue = finalValue ? strdup(finalValue->valuestring) : NULL;
	cJSON_Delete(JSONelement);

end:
	pthread_mutex_unlock(&ritM);
ret:

	return outValue;
}

int rit_tag_base(const char *path, const char *name, const char *value)
{
	cJSON *son, *grandson, *newNode, *tags, *valueNode;
	list_s *splitPath, *ptrList = NULL;
	int retValue = 1;
	int i;

	if (path == NULL || strcmp(path, "") == 0 || name == NULL || strcmp(name, "") == 0 || value == NULL || strcmp(value, "") == 0)
		goto end;

	if (!all) {
		LOG_MSG(LOG__ERROR, false, "tag: Unable to retrieve the RIT");
		goto end;
	}

	splitPath = parse(path, SEPARATOR);
	if (!splitPath) {
		cJSON_Delete(all);
		LOG_MSG(LOG__ERROR, false, "tag: Invalid path");
		goto end;
	}
	ptrList = splitPath;
	i = 0;
	while (ptrList != NULL) {
		if (i != 0) {
			if (cJSON_GetObjectItem(son, ptrList->value) == NULL) {
				grandson = cJSON_CreateObject();
				tags = cJSON_CreateObject();
				cJSON_AddItemToObject(grandson, "tags", tags);
				valueNode = cJSON_CreateString("");
				cJSON_AddItemToObject(grandson, "value", valueNode);
				cJSON_AddItemToObject(son, ptrList->value, grandson);
			}
			++i;
			son = cJSON_GetObjectItem(son, ptrList->value);
		} else {
			if (cJSON_GetObjectItem(all, ptrList->value) == NULL) {
				grandson = cJSON_CreateObject();
				tags = cJSON_CreateObject();
				cJSON_AddItemToObject(grandson, "tags", tags);
				valueNode = cJSON_CreateString("");
				cJSON_AddItemToObject(grandson, "value", valueNode);
				cJSON_AddItemToObject(all, ptrList->value, grandson);
			}
			++i;
			son = cJSON_GetObjectItem(all, ptrList->value);
		}
		ptrList = ptrList->next;
	}

	son = cJSON_GetObjectItem(son, "tags");
	if (!son)
		goto end;
	newNode = cJSON_GetObjectItem(son, name);
	if (newNode)
		cJSON_DeleteItemFromObject(son, name);
	grandson = cJSON_CreateString(value);
	cJSON_AddItemToObject(son, name, grandson);

	clean_list(splitPath);

	retValue = 0;
end:

	return retValue;
}

int rit_var_tag(const char *path, const char *name, const char *value, ...)
{
	int ret;
	int transaction;
	va_list arg_list;

	va_start(arg_list, value);
	transaction = va_arg(arg_list, int);
	va_end(arg_list);

	if (transaction == 1) {
		ret = rit_tag_base(path, name, value);
	} else {
		if (rit_start() != 0) {
			ret = 1;
			goto end;
		}
		ret = rit_tag_base(path, name, value);
		rit_commit();
	}
end:
	return ret;
}

int rit_untag_base(const char *path, const char *name)
{
	cJSON *son, *aux;
	list_s *splitPath, *ptrList = NULL;
	int i = 0, retValue = 1;

	if (path == NULL || strcmp(path, "") == 0 || name == NULL || strcmp(name, "") == 0)
		goto end;

	if (!all) {
		LOG_MSG(LOG__ERROR, false, "untag: Unable to retrieve the RIT");
		goto end;
	}

	splitPath = parse(path, SEPARATOR);
	if (!splitPath) {
		cJSON_Delete(all);
		LOG_MSG(LOG__ERROR, false, "untag: Invalid path");
		goto end;
	}
	ptrList = splitPath;
	while (ptrList != NULL) {
		son = i ? cJSON_GetObjectItem(son, ptrList->value) : cJSON_GetObjectItem(all, ptrList->value);
		if (!son)
			goto end;
		ptrList = ptrList->next;
		++i;
	}

	son = cJSON_GetObjectItem(son, "tags");
	if (!son)
		goto end;
	aux = cJSON_GetObjectItem(son, name);
	if (aux != NULL)
		cJSON_DeleteItemFromObject(son, name);

	clean_list(splitPath);

	retValue = 0;
end:

	return retValue;
}

int rit_var_untag(const char *path, const char *value, ...)
{
	int ret;
	int transaction;
	va_list arg_list;

	va_start(arg_list, value);
	transaction = va_arg(arg_list, int);
	va_end(arg_list);

	if (transaction == 1) {
		ret = rit_untag_base(path, value);
	} else {
		if (rit_start() != 0) {
			ret = 1;
			goto end;
		}
		ret = rit_untag_base(path, value);
		rit_commit();
	}
end:

	return ret;
}

/* Search */
char *rit_getPathsByTag(const char *name, const char *value)
{
	cJSON *root;
	char *paths = NULL, *rootPath, *complete_path = NULL;

	if (value == NULL || strcmp(value, "") == 0 || name == NULL || strcmp(name, "") == 0)
		goto ret;
	pthread_mutex_lock(&ritM);

	rootPath = strdup(ROOT);
	root = get_element_by_path(rootPath);
	if (!root) {
		free(rootPath);
		goto end;
	}
	paths = rit_search(root, name, value, &complete_path);
	if (!paths) {
		free(rootPath);
		cJSON_Delete(root);
		goto end;
	}

	free(rootPath);
	cJSON_Delete(root);

end:
	pthread_mutex_unlock(&ritM);
ret:

	return paths;
}


char *rit_getTagNamesByPath(const char *path)
{
	cJSON *JSONelement;
	char *outValue = NULL;

	if (path == NULL || strcmp(path, "") == 0)
		goto ret;
	pthread_mutex_lock(&ritM);

	JSONelement = get_element_by_path(path);
	if (!JSONelement) goto end;
	outValue = get_tag_names_from_JSON(JSONelement);

	cJSON_Delete(JSONelement);

end:
	pthread_mutex_unlock(&ritM);
ret:

	return outValue;
}

char *rit_getChilds(const char *path)
{
	cJSON *JSONelement, *son;
	char *outValue = NULL;
	int arraySize = 0, i = 0;
	int len = 0, off = 0;

	pthread_mutex_lock(&ritM);

	JSONelement = get_element_by_path(path);
	if (!JSONelement)
		goto end;
	arraySize = cJSON_GetArraySize(JSONelement);
	while (i < arraySize) {
		son = cJSON_GetArrayItem(JSONelement, i);
		if ((strcmp(son->string, "tags") != 0) && (strcmp(son->string, "value") != 0)) {
			off = outValue ? strlen(outValue) : 0;
			len = off + strlen(son->string) + 1 + 1;
			char *new_outValue = (char *)realloc(outValue, len);
			outValue = new_outValue ? new_outValue : outValue;
			if (off) {
				strncpy(outValue + off, ";", 1);
				++off;
			}
			strncpy(outValue + off, son->string, len - off);
		}
		++i;
	}

	cJSON_Delete(JSONelement);

end:
	pthread_mutex_unlock(&ritM);

	return outValue;
}

/* Clean */
int rit_clear_base(const char *path)
{
	char *allTags;
	list_s *splitTags, *ptrList = NULL;
	int retValue = 0;

	if (path == NULL || strcmp(path, "") == 0) {
		retValue = 1;
		goto end;
	}
	rit_unset_base(path);

	allTags = rit_getTagNamesByPath(path);
	if (!allTags)
		return -1;
	splitTags = parse(allTags, ";");
	ptrList = splitTags;
	while (ptrList != NULL) {
		if (rit_untag_base(path, ptrList->value) != 0) {
			retValue = 1;
			break;
		}
		ptrList = ptrList->next;
	}


	free(allTags);
	clean_list(splitTags);
end:

	return retValue;
}

int rit_var_clear(const char *path, ...)
{
	int ret;
	int transaction;
	va_list arg_list;

	va_start(arg_list, path);

	transaction = va_arg(arg_list, int);
	va_end(arg_list);

	if (transaction == 1) {
		ret = rit_clear_base(path);
	} else {
		if (rit_start() != 0) {
			ret = 1;
			goto end;
		}
		ret = rit_clear_base(path);
		rit_commit();
	}
end:

	return ret;
}

int rit_delete_base(const char *path)
{
	cJSON *son = NULL;
	list_s *splitPath, *ptrList = NULL;
	int i = 0, retValue = 1;

	if (!path)
		goto end;
	if (strcmp(path, "/") == 0 || strcmp(path, "") == 0)
		goto end;

	if (!all)
		goto end;
	splitPath = parse(path, SEPARATOR);
	if (!splitPath) {
		cJSON_Delete(all);
		goto end;
	}
	ptrList = splitPath;
	while (ptrList->next != NULL) {
		son = i ? cJSON_GetObjectItem(son, ptrList->value) : cJSON_GetObjectItem(all, ptrList->value);
		if (!son)
			goto end;
		ptrList = ptrList->next;
		++i;
	}

	if (son)
		cJSON_DeleteItemFromObject(son, ptrList->value);
	else
		cJSON_DeleteItemFromObject(all, ptrList->value);

	clean_list(splitPath);

	retValue = 0;
end:

	return retValue;
}

int rit_var_delete(const char *path, ...)
{
	int ret;
	int transaction;
	va_list arg_list;

	va_start(arg_list, path);
	transaction = va_arg(arg_list, int);
	va_end(arg_list);

	if (transaction == 1) {
		ret = rit_delete_base(path);
	} else {
		if (rit_start() != 0) {
			ret = 1;
			goto end;
		}
		ret = rit_delete_base(path);
		rit_commit();
	}
end:
	return ret;
}

void rit_drop()
{
	rit_start();

	cJSON *tags, *newNode;

	cJSON_Delete(all);
	all = NULL;
	all = cJSON_CreateObject();
	tags = cJSON_CreateObject();
	newNode = cJSON_CreateString("");
	cJSON_AddItemToObject(all, "tags", tags);
	cJSON_AddItemToObject(all, "value", newNode);

	rit_commit();
}
