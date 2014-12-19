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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "include/world.h"

#include "modules/include/hash.h"
#include "common/include/bundle.h"

// Hash tables
static bundle_code_dl_s *g_bundle_code_dl_ht = NULL;
static routing_code_dl_s *g_routing_code_dl_ht = NULL;
static prio_code_dl_s *g_prio_code_dl_ht = NULL;
static life_code_dl_s *g_life_code_dl_ht = NULL;

// Helpers
bundle_code_dl_s *bundle_dl_add(const char *id, bundle_info_s *info, dl_s *dls)
{
	bundle_code_dl_s *new_bundle = NULL;
	int ret = 0;

	if (id == NULL || *id == '\0') {
		ret = 1;
		goto end;
	}
	new_bundle = (bundle_code_dl_s *)calloc(1, sizeof(bundle_code_dl_s));

	new_bundle->bundle_id = strdup(id);
	if (info)
		memcpy(&new_bundle->info, info, sizeof(new_bundle->info));
	if (dls)
		memcpy(&new_bundle->dls, dls, sizeof(new_bundle->dls));

	HASH_ADD_KEYPTR(hh, g_bundle_code_dl_ht, new_bundle->bundle_id, strlen(new_bundle->bundle_id), new_bundle);

end:
	if (ret) {
		if (new_bundle)
			free(new_bundle);
		new_bundle = NULL;
	}

	return new_bundle;
}

bundle_code_dl_s *bundle_dl_find(const char *id)
{
	bundle_code_dl_s *b_dl = NULL;

	if (id == NULL || *id == '\0') {
		goto end;
	}

	HASH_FIND_STR(g_bundle_code_dl_ht, id, b_dl);
end:

	return b_dl;
}

int bundle_dl_remove(bundle_code_dl_s *b_dl)
{
	int ret = 0;

	if (b_dl == NULL) {
		ret = 1;
		goto end;
	}

	HASH_DELETE(hh, g_bundle_code_dl_ht, b_dl);
end:

	return ret;
}

// Applies provided function to all bundle_dl elements stored in the g_bundle_code_dl_ht hash table
int del_map_bundle_dl_table( int (*f)(bundle_code_dl_s *))
{
	int ret = 0;
	bundle_code_dl_s *curr = NULL, *tmp = NULL;

	HASH_ITER(hh, g_bundle_code_dl_ht, curr, tmp) {
		ret = ret || f(curr);
		HASH_DEL(g_bundle_code_dl_ht, curr);
		free(curr);
	}

	return ret;
}


static void *code_add_dl(const char *code, void *dl, const code_type_e code_type)
{
	void *ret = NULL;

	if (code == NULL || *code == '\0') {
		goto end;
	} else if (code_type == ROUTING_CODE) {
		routing_code_dl_s *r_dl = (routing_code_dl_s *)calloc(1, sizeof(routing_code_dl_s));
		r_dl->code = strdup(code);
		if (dl)
			memcpy(&r_dl->dl, dl, sizeof(r_dl->dl));
		HASH_ADD_KEYPTR(hh, g_routing_code_dl_ht, r_dl->code, strlen(r_dl->code), r_dl);
		ret = (void *)r_dl;
	} else if (code_type == PRIO_CODE) {
		prio_code_dl_s *p_dl = (prio_code_dl_s *)calloc(1, sizeof(prio_code_dl_s));
		p_dl->code = strdup(code);
		if (dl)
			memcpy(&p_dl->dl, dl, sizeof(p_dl->dl));
		HASH_ADD_KEYPTR(hh, g_prio_code_dl_ht, p_dl->code, strlen(p_dl->code), p_dl);
		ret = (void *)p_dl;
	} else if (code_type == LIFE_CODE) {
		life_code_dl_s *l_dl = (life_code_dl_s *)calloc(1, sizeof(life_code_dl_s));
		l_dl->code = strdup(code);
		if (dl)
			memcpy(&l_dl->dl, dl, sizeof(l_dl->dl));
		HASH_ADD_KEYPTR(hh, g_life_code_dl_ht, l_dl->code, strlen(l_dl->code), l_dl);
		ret = (void *)l_dl;
	}
end:

	return ret;
}

routing_code_dl_s *routing_code_dl_add(const char *code, routing_dl_s *dl)
{
	return (routing_code_dl_s *) code_add_dl(code, dl, ROUTING_CODE);
}

life_dl_s *life_code_dl_add(const char *code, life_dl_s *dl)
{
	return (life_dl_s *) code_add_dl(code, dl, LIFE_CODE);
}

prio_dl_s *prio_code_dl_add(const char *code, prio_dl_s *dl)
{
	return (prio_dl_s *) code_add_dl(code, dl, PRIO_CODE);
}

static void *code_dl_find(const char *code, const code_type_e code_type)
{
	void *ret = NULL;

	if (code == NULL || *code == '\0')
		goto end;
	else if (code_type == ROUTING_CODE) {
		routing_code_dl_s *r_dl = NULL;
		HASH_FIND(hh, g_routing_code_dl_ht, code, strlen(code), r_dl);
		ret = r_dl;
	} else if (code_type == PRIO_CODE) {
		prio_code_dl_s *p_dl = NULL;
		HASH_FIND(hh, g_prio_code_dl_ht, code, strlen(code), p_dl);
		ret = p_dl;
	} else if (code_type == LIFE_CODE) {
		life_code_dl_s *l_dl = NULL;
		HASH_FIND(hh, g_life_code_dl_ht, code, strlen(code), l_dl);
		ret = l_dl;
	}
end:

	return ret;
}

routing_code_dl_s *routing_code_dl_find(const char *code)
{
	return (routing_code_dl_s *) code_dl_find(code, ROUTING_CODE);
}

prio_code_dl_s *prio_code_dl_find(const char *code)
{
	return (prio_code_dl_s *) code_dl_find(code, PRIO_CODE);
}

life_code_dl_s *life_code_dl_find(const char *code)
{
	return (life_code_dl_s *) code_dl_find(code, LIFE_CODE);
}

static int code_dl_remove(void *code_dl, const code_type_e code_type)
{
	int ret = 0;
	routing_code_dl_s *routing_code_dl = NULL;
	prio_code_dl_s *prio_code_dl = NULL;
	life_code_dl_s *life_code_dl = NULL;

	if (code_dl == NULL)
		goto end;
	else if (code_type == ROUTING_CODE) {
		routing_code_dl = (routing_code_dl_s *)code_dl;
		HASH_DELETE(hh, g_routing_code_dl_ht, routing_code_dl);
	} else if (code_type == PRIO_CODE) {
		prio_code_dl = (prio_code_dl_s *) code_dl;
		HASH_DELETE(hh, g_prio_code_dl_ht, prio_code_dl);
	} else if (code_type == LIFE_CODE) {
		life_code_dl = (life_code_dl_s *)code_dl;
		HASH_DELETE(hh, g_life_code_dl_ht, life_code_dl);
	}
end:

	return ret;
}

int routing_code_dl_remove(void *code_dl)
{
	return code_dl_remove(code_dl, ROUTING_CODE);
}

int life_code_dl_remove(void *code_dl)
{
	return code_dl_remove(code_dl, LIFE_CODE);
}

int prio_code_dl_remove(void *code_dl)
{
	return code_dl_remove(code_dl, PRIO_CODE);
}

