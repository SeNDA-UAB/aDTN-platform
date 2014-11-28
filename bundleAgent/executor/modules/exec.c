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

#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "include/world.h"

#include "common/include/log.h"
#include "common/include/neighbours.h"

#include "modules/include/exec.h"
#include "modules/include/hash.h"
#include "modules/include/codes.h"

//#define CODES_PREFIX "/tmp/"

#define HELPERS_PATH "/adtn/exec_helpers"
#define HELPERS_PREFIX LIB_PREFIX""HELPERS_PATH
#define HELPERS_INCLUDE_PATH HELPERS_PREFIX"/include"

// Routing
#define COMPILE_ROUTING_CMD "gcc -w -fPIC -I %s -shared %s %s  -Wl,--whole-archive %s -Wl,--no-whole-archive -o %s.so"
#define R_HELPER_PATH HELPERS_PREFIX"/libadtnrhelper.a"
#define RIT_HELPER_PATH LIB_PREFIX"/libadtnrithelper.so"
#define R_HEADER "#include \"adtnrhelper.h\"\n#include \"adtnrithelper.h\"\n"

// No helpers so
#define COMPILE_NO_HELPERS_CMD "gcc -w -fPIC -shared  -Wl,--whole-archive %s -Wl,--no-whole-archive -o %s.so"

static char *get_code_path(const char *name, const code_type_e type)
{
	int code_path_l = strlen(world.objects_path) + strlen(name) + 6;
	char *code_path = (char *)malloc(code_path_l);
	snprintf(code_path, code_path_l, "%s/%s_%d.c", world.objects_path, name, type);

	return code_path;
}

char *get_so_path(const char *name, const code_type_e type)
{
	int code_path_l = strlen(world.objects_path) + strlen(name) + 7;
	char *code_path = (char *)malloc(code_path_l);
	snprintf(code_path, code_path_l, "%s/%s_%d.so", world.objects_path, name, type);

	return code_path;
}

static int write_code_to_disk(const char *code, const char *code_path)
{
	int ret = 0;
	FILE *fd = fopen(code_path, "w");
	if (fd == NULL || fwrite(code, sizeof(char), strlen(code), fd) == -1) {
		ret = 1;
		LOG_MSG(LOG__ERROR, true, "Error writing code %s to disk", code_path);
		goto end;
	}

end:
	fclose(fd);
	return ret;
}

static int remove_code_from_disk(const char *code_path)
{
	int ret = 0;
	if (unlink(code_path) != 0) {
		ret = 1;
		LOG_MSG(LOG__ERROR, true, "Error removing code %s from disk", code_path);
	}

	return ret;
}


static int execute_compile_cmd(const char *cmd)
{
	int ret = 0;

	ret = system(cmd);
	if (ret != 0)
		LOG_MSG(LOG__ERROR, true, "Error executing compile command %s", cmd);

	return ret;
}

/* Routing */
static char *get_compile_routing_cmd(const char *code_path, const char *so_path)
{
	int comand_l =
	    strlen(COMPILE_ROUTING_CMD) +
	    strlen(HELPERS_INCLUDE_PATH) +
	    strlen(code_path) +
	    strlen(RIT_HELPER_PATH) +
	    strlen(R_HELPER_PATH) +
	    strlen(so_path) + 1;
	char *command = (char *)malloc(comand_l);

	snprintf(command, comand_l,
	         COMPILE_ROUTING_CMD,
	         HELPERS_INCLUDE_PATH,
	         code_path,
	         RIT_HELPER_PATH,
	         R_HELPER_PATH,
	         so_path
	        );

	return command;
}

static int compile_routing_code(const char *code_name, const char *code)
{
	int ret = 0, code_ready_l = 0;
	char *cmd = NULL, *code_ready  = NULL, *code_path = NULL, *so_path = NULL;

	// Prepare code
	code_ready_l = strlen(R_HEADER) + strlen(code) + 2;
	code_ready = (char *)malloc(code_ready_l * sizeof(char));

	snprintf(code_ready, code_ready_l, "%s%s", R_HEADER, code);

	code_path = get_code_path(code_name, ROUTING_CODE);
	if (write_code_to_disk(code_ready, code_path) != 0) {
		ret = 1;
		goto end;
	}

	so_path = get_so_path(code_name, ROUTING_CODE);
	so_path[strlen(so_path) - 3] = '\0';                    // Remove .so extension

	cmd = get_compile_routing_cmd(code_path, so_path);

	if (execute_compile_cmd(cmd) != 0) {
		ret = 1;
		goto end;
	}

	remove_code_from_disk(code_path);

end:
	if (ret == 1)
		LOG_MSG(LOG__ERROR, false, "Error compiling routing code");

	if (code_ready)
		free(code_ready);
	if (code_path)
		free(code_path);
	if (so_path)
		free(so_path);
	if (cmd)
		free(cmd);

	return ret;
}

static routing_dl_s *load_routing_dl(const char *so_name)
{
	int ret = 0;
	routing_dl_s *routing_dl = (routing_dl_s *)calloc(1, sizeof(routing_dl_s));
	char *dl_path = NULL;
	void (*init_env)();

	dl_path = get_so_path(so_name, ROUTING_CODE);

	*(void **)&routing_dl->handler = dlopen(dl_path, RTLD_LAZY | RTLD_LOCAL);
	if (routing_dl->handler == NULL) {
		LOG_MSG(LOG__ERROR, false, "%s: Error opening: %s", dlerror(), dl_path);
		ret = 1;
		goto end;
	}

	*(void **)&routing_dl->r = dlsym(routing_dl->handler, "r");
	if (routing_dl->r == NULL) {
		LOG_MSG(LOG__ERROR, false, "%s: Error loading r symbol", dlerror());
		ret = 1;
		goto end;
	}

	*(void **)&routing_dl->nbs_info = dlsym(routing_dl->handler, "nbs_info");
	if (routing_dl->nbs_info == NULL) {
		LOG_MSG(LOG__ERROR, false, "%s: Error loading nbs_info symbol", dlerror());
		ret = 1;
		goto end;
	}

	*(void **)&routing_dl->dest = dlsym(routing_dl->handler, "dest");
	if (routing_dl->dest == NULL) {
		LOG_MSG(LOG__ERROR, false, "%s: Error loading dest symbol", dlerror());
		ret = 1;
		goto end;
	}

	*(void **)&routing_dl->r_result = dlsym(routing_dl->handler, "r_result");
	if (routing_dl->r_result == NULL) {
		LOG_MSG(LOG__ERROR, false, "%s: Error loading r_result symbol", dlerror());
		ret = 1;
		goto end;
	}

	*(void **) (&init_env) = dlsym(routing_dl->handler, "init_env");
	if (init_env == NULL) {
		LOG_MSG(LOG__ERROR, false, "%s: Error loading init_env symbol", dlerror());
		ret = 1;
		goto end;
	}

	// Prepare loaded dl
	init_env();

end:
	if (ret) {
		if (routing_dl->handler != NULL)
			dlclose(routing_dl->handler);
		free(routing_dl);
		routing_dl = NULL;
	}

	if (dl_path)
		free(dl_path);

	return routing_dl;
}

// -1 -> Error, 0 -> Code was already loaded, 1-> New code, 2-> Default code
// bundle dl info copied to bundle_code_dl if it is not NULL
int prepare_routing_code(const char *bundle_id, bundle_code_dl_s **bundle_code_dl)
{
	bundle_info_s target_bundle_info = {0};
	bundle_code_dl_s *target_bundle_dl = NULL;
	routing_code_dl_s *routing_code_dl = NULL;
	routing_dl_s *routing_dl = NULL;
	dl_s target_bundle_dls = {0};

	char *routing_code = NULL;
	int routing_code_l = 0, ret = 0;

	// Find if code from this bundle has already been compiled and loaded
	if ((target_bundle_dl = bundle_dl_find(bundle_id)) == NULL || target_bundle_dl->dls.routing == NULL) {
		// If not, get code and find if code has already been compiled for some other bundle
		if (get_code_and_info(ROUTING_CODE, bundle_id, &routing_code, &routing_code_l, &target_bundle_info) != 0) {
			goto end;
		}

		if ((routing_code_dl = routing_code_dl_find(routing_code)) == NULL) {
			// If not, compile it, load it and add it to the hash dl table
			if (compile_routing_code(bundle_id, routing_code) != 0) {
				goto end;
			}
			if ((routing_dl = load_routing_dl(bundle_id)) == NULL ) {
				goto end;
			}
			if ((routing_code_dl = routing_code_dl_add(routing_code, routing_dl)) == NULL) {
				goto end;
			}
			routing_code_dl->dl = routing_dl;
			pthread_mutex_init(&routing_code_dl->exec, NULL);
			ret = 1;
		}

		// link compiled code with target bundle and add new bundle to bundle hash table
		target_bundle_dls.routing = routing_code_dl;
		routing_code_dl->refs++;
		if ( (target_bundle_dl = bundle_dl_add(bundle_id, &target_bundle_info, &target_bundle_dls)) == NULL) {
			goto end;
		}
	}
end:
	if (routing_code)
		free(routing_code);

	if (target_bundle_dl == NULL || target_bundle_dl->dls.routing == NULL)
		ret = -1;
	else if (bundle_code_dl != NULL)
		*bundle_code_dl = target_bundle_dl;

	return ret;
}

int execute_routing_code(routing_dl_s *routing_dl, char *dest, routing_exec_result *result)
{
	int ret = 0;

	// Reset nb iterator
	routing_dl->nbs_info->position = 0;
	routing_dl->nbs_info->nbs_list_l = get_nbs(&routing_dl->nbs_info->nbs_list);

	*routing_dl->dest = dest;
	*routing_dl->r_result = result;
	// Execute code
	ret = routing_dl->r();

	if (routing_dl->nbs_info->nbs_list_l != 0)
		free(routing_dl->nbs_info->nbs_list);

	return ret;

}
/**/


/* No helpers */
static char *get_no_helpers_routing_cmd(code_type_e code_type, const char *code_path, const char *so_path)
{
	int comand_l =
	    strlen(COMPILE_NO_HELPERS_CMD) +
	    strlen(code_path) +
	    strlen(so_path) + 1;
	char *command = (char *)malloc(comand_l);

	snprintf(command, comand_l,
	         COMPILE_NO_HELPERS_CMD,
	         code_path,
	         so_path
	        );

	return command;
}

static int compile_no_helpers_code(code_type_e code_type, const char *code_name, const char *code)
{
	int ret = 0;
	char *cmd = NULL, *code_path = NULL, *so_path = NULL;

	code_path = get_code_path(code_name, code_type);
	if (write_code_to_disk(code, code_path) != 0) {
		ret = 1;
		goto end;
	}

	so_path = get_so_path(code_name, code_type);
	so_path[strlen(so_path) - 3] = '\0';                    // Remove .so extension

	cmd = get_no_helpers_routing_cmd(code_type, code_path, so_path);

	if (execute_compile_cmd(cmd) != 0) {
		ret = 1;
		goto end;
	}

	remove_code_from_disk(code_path);

end:
	if (ret == 1)
		LOG_MSG(LOG__ERROR, false, "Error compiling %s", code_type_e_name[code_type]);

	if (code_path)
		free(code_path);
	if (so_path)
		free(so_path);
	if (cmd)
		free(cmd);

	return ret;
}

static void *load_no_helpers_dl(code_type_e code_type, const char *so_name)
{

	int ret = 0;
	char *dl_path = NULL;
	const char *main_funct_name = NULL;
	void **handler = NULL, *ret_p = NULL;
	int (**main_funct)(void) = NULL;
	prio_dl_s *prio_dl = NULL;
	life_dl_s *life_dl = NULL;

	switch (code_type) {
	case PRIO_CODE:
		prio_dl = (prio_dl_s *)calloc(1, sizeof(prio_dl_s));
		handler = &prio_dl->handler;
		main_funct = &prio_dl->p;
		main_funct_name = "p";
		ret_p = (void *)prio_dl;
		break;
	case LIFE_CODE:
		life_dl = (life_dl_s *)calloc(1, sizeof(life_dl_s));
		handler = &life_dl->handler;
		main_funct = &life_dl->l;
		main_funct_name = "l";
		ret_p = (void *)life_dl;
		break;
	default :
		ret = 1;
		LOG_MSG(LOG__ERROR, false, "Error: load_no_helpers_dl() the type of the code to laod is not recognized");
		goto end;
	}

	dl_path = get_so_path(so_name, code_type);

	*(void **)(handler) = dlopen(dl_path, RTLD_LAZY | RTLD_LOCAL);
	if (*handler == NULL) {
		LOG_MSG(LOG__ERROR, false, "%s: Error opening: %s", dlerror(), dl_path);
		ret = 1;
		goto end;
	}

	*(void **)(main_funct) = dlsym(*handler, main_funct_name);
	if (*main_funct == NULL) {
		LOG_MSG(LOG__ERROR, false, "%s: Error loading %s symbol", dlerror(), main_funct_name);
		ret = 1;
		goto end;
	}


end:
	if (ret) {
		if (ret_p) {
			free(ret_p);
			ret_p = NULL;
		}

		if (*handler)
			dlclose(*handler);
	}
	if (dl_path)
		free(dl_path);

	return ret_p;
}

// -1 -> Error, 0 -> Code was already loaded, 1-> New bundle.
// bundle dl info copied to bundle_code_dl if it is not NULL
int prepare_no_helpers_code(code_type_e code_type, const char *bundle_id, bundle_code_dl_s **bundle_code_dl)
{
	char *code = NULL;
	int code_l = 0, ret = 0, check_code = 0;

	bundle_code_dl_s *target_bundle_dl = NULL;
	dl_s target_bundle_dls = {0};

	prio_code_dl_s *prio_code_dl = NULL;
	life_code_dl_s *life_code_dl = NULL;
	void **code_dl = NULL;

	prio_dl_s *prio_dl = NULL;
	life_dl_s *life_dl = NULL;
	void **dl = NULL;

	void *(*code_dl_find)(const char *);
	void *(*code_dl_add)(const char *, void *);


	// Find if code from this bundle has already been compiled and loaded
	target_bundle_dl = bundle_dl_find(bundle_id);
	if (target_bundle_dl == NULL) {
		check_code = 1;
	} else {
		switch (code_type) {
		case LIFE_CODE:
			if (target_bundle_dl->dls.life == NULL)
				check_code = 1;
			break;
		case PRIO_CODE:
			if (target_bundle_dl->dls.prio == NULL)
				check_code = 1;
			break;
		default:
			LOG_MSG(LOG__ERROR, false, "prepare_no_helpers_code(): Code to prepare not recognized");
			goto end;
		}
	}

	if (check_code) {
		// If not, get code and find if code has already been compiled for some other bundle
		if (get_code_and_info(code_type, bundle_id, &code, &code_l, NULL) != 0) {
			goto end;
		}

		switch (code_type) {
		case LIFE_CODE:
			code_dl = (void **)&life_code_dl;
			dl = (void **)&life_dl;
			code_dl_find = (void *(*)(const char *))life_code_dl_find;
			code_dl_add = (void *(*)(const char *, void *))life_code_dl_add;
			break;
		case PRIO_CODE:
			code_dl = (void **)&prio_code_dl;
			dl = (void **)&prio_dl;
			code_dl_find = (void *(*)(const char *))prio_code_dl_find;
			code_dl_add = (void *(*)(const char *, void *))prio_code_dl_add;
			break;
		default:
			LOG_MSG(LOG__ERROR, false, "prepare_no_helpers_code(): Code to prepare not recognized");
			ret = -1;
			goto end;
		}


		if ((*code_dl = code_dl_find(code)) == NULL) {
			// If not, compile it, load it and add it to the hash dl table
			if (compile_no_helpers_code(code_type, bundle_id, code) != 0) {
				goto end;
			}

			if ((*dl = load_no_helpers_dl(code_type, bundle_id)) == NULL ) {
				goto end;
			}
			if ((*code_dl = code_dl_add(code, dl)) == NULL) {
				goto end;
			}
			ret = 1;
		}

		// link compiled code with target bundle and add new bundle to bundle hash table
		switch (code_type) {
		case LIFE_CODE:
			target_bundle_dls.life = life_code_dl;
			target_bundle_dls.life ->refs++;
			break;
		case PRIO_CODE:
			target_bundle_dls.prio = prio_code_dl;
			target_bundle_dls.prio ->refs++;
			break;
		default:
			LOG_MSG(LOG__ERROR, false, "prepare_no_helpers_code(): Code to prepare not recognized");
			ret = -1;
			break;
		}

		if ( (target_bundle_dl = bundle_dl_add(bundle_id, NULL, &target_bundle_dls)) == NULL) {
			goto end;
		}
	}

end:

	if (code)
		free(code);

	switch (code_type) {
	case LIFE_CODE:
		if (target_bundle_dl == NULL || target_bundle_dl->dls.life == NULL)
			ret = -1;
		break;
	case PRIO_CODE:
		if (target_bundle_dl == NULL || target_bundle_dl->dls.prio == NULL)
			ret = -1;
		break;
	default:
		ret = -1;
		break;
	}

	if (bundle_code_dl != NULL)
		*bundle_code_dl = target_bundle_dl;

	return ret;
}

int execute_no_helpers_code(code_type_e code_type, void *code_dl, /*OUT*/ int *result)
{
	int ret = 0;

	switch (code_type) {
	case LIFE_CODE:
		*result = ((life_dl_s *)code_dl)->l();
		break;
	case PRIO_CODE:
		*result = ((prio_dl_s *)code_dl)->p();
		break;
	default:
		LOG_MSG(LOG__ERROR, false, "execute_no_helpers_code(): Code type to executo not recognized");
		goto end;
	}

end:
	return ret;
}

/**/