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

#ifndef H_ADTNRITHLPER_INC
#define H_ADTNRITHLPER_INC

// #define ID_LEN 23

typedef struct _routing_exec_result {
	char *hops_list;
	int hops_list_l;
} routing_exec_result;

typedef struct _iter_info {
	char *nbs_list;
	short int nbs_list_l;
	short int position;
} nbs_iter_info;

typedef struct _nbs_iterator {
	char *(*start)(void);
	int *(*has_next)(void);
	char *(*next)(void);
	int (*num)(void);
	char *(*nb)(int);
} nbs_iterator;

extern nbs_iterator nbs;
extern char *dest;
extern char *prev_hop;

void add_hop(const char *hop);
// void rm_hop();

#endif
