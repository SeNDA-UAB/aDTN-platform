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

#ifndef INC_HASH_H
#define INC_HASH_H

#include <pthread.h>
#include "modules/exec_c_helpers/include/adtnrhelper.h"

#include "common/include/uthash.h"

/* Dl structs*/
typedef struct _routing_dl_s {
	void *handler;
	int (*r)(void);
	nbs_iter_info *nbs_info;
	routing_exec_result **r_result;
	char **prev_hop;
	char **dest;
} routing_dl_s;

typedef struct _prio_dl_s {
	void *handler;
	int (*p)(void);
} prio_dl_s;

typedef struct _life_dl {
	void *handler;
	int (*l)(void);
} life_dl_s;
/**/

/* Hash tables structs*/
typedef struct _routing_code_dl_s {
	const char *code; // Key
	int refs;
	routing_dl_s *dl;
	pthread_mutex_t exec;
	UT_hash_handle hh;
} routing_code_dl_s;

typedef struct _prio_code_dl_s {
	const char *code; // Key
	int refs;
	prio_dl_s *dl;
	pthread_mutex_t exec;
	UT_hash_handle hh;
} prio_code_dl_s;

typedef struct _life_code_dl_s {
	const char *code; // Key
	int refs;
	life_dl_s *dl;
	pthread_mutex_t exec;
	UT_hash_handle hh;
} life_code_dl_s;

typedef struct _dl_s {
	routing_code_dl_s *routing;
	prio_code_dl_s *prio;
	life_code_dl_s *life;
} dl_s;

typedef struct _bundle_info_s {
	char *prev_hop;
	char *dest;
} bundle_info_s;

typedef struct _bundle_code_dl_s {
	const char *bundle_id; // Key
	bundle_info_s info;
	dl_s dls;
	UT_hash_handle hh;
} bundle_code_dl_s;
/**/

bundle_code_dl_s *bundle_dl_add(const char *id, bundle_info_s *info, dl_s *dls);
bundle_code_dl_s *bundle_dl_find(const char *id);
int bundle_dl_remove(bundle_code_dl_s *b_dl);
int del_map_bundle_dl_table( int (*f)(bundle_code_dl_s *));

routing_code_dl_s *routing_code_dl_add(const char *code, routing_dl_s *dl);
life_dl_s *life_code_dl_add(const char *code, life_dl_s *dl);
prio_dl_s *prio_code_dl_add(const char *code, prio_dl_s *dl);

routing_code_dl_s *routing_code_dl_find(const char *code);
life_code_dl_s *life_code_dl_find(const char *code);
prio_code_dl_s *prio_code_dl_find(const char *code);

int routing_code_dl_remove(void *code_dl);
int life_code_dl_remove(void *code_dl);
int prio_code_dl_remove(void *code_dl);

#endif