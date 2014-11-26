#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "include/world.h"
#include "include/executor.h"
#include "modules/include/hash.h"

#include "common/include/log.h"
#include "common/include/bundle.h"
#include "common/include/minIni.h"

static int open_raw_bundle(const char *path, const char *bundle_id, /*out*/uint8_t **bundle_raw)
{
	int ret = 0, bundle_path_l = 0;
	long bundle_raw_l = 0;
	char *bundle_path = NULL;
	FILE *bundle_fd = NULL;

	bundle_path_l = strlen(path) + 1 + strlen(bundle_id) + 1;
	bundle_path = (char *)malloc(bundle_path_l);
	snprintf(bundle_path, bundle_path_l, "%s/%s", path, bundle_id);

	// Get bundle from disk
	bundle_fd = fopen(bundle_path, "r");
	if (bundle_fd == NULL) {
		ret = 1;
		goto end;
	}
	if (fseek(bundle_fd, 0, SEEK_END) != 0) {
		ret = 1;
		goto end;
	}
	bundle_raw_l = ftell(bundle_fd);
	if (fseek(bundle_fd, 0, SEEK_SET) != 0) {
		ret = 1;
		goto end;
	}
	*bundle_raw = (uint8_t *)malloc(bundle_raw_l);
	if (fread(*bundle_raw, bundle_raw_l, 1, bundle_fd)  <= 0) {
		ret = 1;
		goto end;
	}
end:
	if (bundle_path != NULL)
		free(bundle_path);
	if (bundle_fd != NULL)
		fclose(bundle_fd);

	return ret;
}

static int get_bundle_content(const uint8_t *bundle_raw, /*out*/bundle_info_s *info, /*out*/mmeb_body_s *mmeb)
{
	int ret = 0;

	if (info != NULL) {
		if (bundle_get_destination(bundle_raw, (uint8_t **)&info->dest) != 0) {
			LOG_MSG(LOG__ERROR, false, "Can't get destination.");
			ret = 1;
			goto end;
		}
	}

	if (bundle_raw_get_mmeb(bundle_raw, mmeb) != 0) {
		LOG_MSG(LOG__ERROR, false, "Can't get mmeb block of bundle");
		ret = 1;
		goto end;
	}
end:

	return ret;
}

static int find_code(mmeb_body_s *mmeb, code_type_e type, /*out*/char **code, /*out*/int *code_l)
{
	int ret = 0;
	mmeb_code_s *next_code = NULL;
	mmeb_body_s *next_mmeb = NULL;

	*code = NULL;

	for (next_mmeb = mmeb; next_mmeb != NULL; next_mmeb = next_mmeb->next) {
		if (next_mmeb->alg_type == type) {
			for (next_code = next_mmeb->code; next_code != NULL; next_code = next_code->next) {
				if (next_code->fwk == FWK) {
					*code_l = next_code->sw_length + 1;
					*code = (char *)calloc(1, *code_l);
					memcpy(*code, next_code->sw_code, *code_l - 1);
					(*code)[*code_l - 1] = '\0'; //Check
				}
			}

			break;
		}
	}

	if (*code == NULL)
		ret = 1;

	return ret;
}

/* From receiver */
static int get_file_size(FILE *fd)
{
	int total_bytes;

	if (fd == NULL)
		return 0;
	fseek(fd, 0L, SEEK_END);
	total_bytes = (int)ftell(fd);
	rewind(fd);

	return total_bytes;
}

static int load_code_from_file(const char *path, char **code)
{
	FILE *fd = NULL;
	int code_l = 0;
	int ret = -1;

	if (code == NULL)
		goto end;

	if ((fd = fopen(path, "r")) <= 0) {
		LOG_MSG(LOG__ERROR, true, "Error loading code %s", path);
		goto end;
	}

	if ((code_l = get_file_size(fd)) <= 0) {
		LOG_MSG(LOG__ERROR, false, "Error getting lenght of code %s", path);
		goto end;
	}

	*code = (char *) malloc(sizeof(**code) * (code_l + 1));
	if (fread(*code, sizeof(**code), code_l, fd) != code_l) {
		LOG_MSG(LOG__ERROR, true, "Error reading code %s", path);
		goto end;
	}
	(*code)[code_l] = '\0';

	ret = code_l;
end:
	if (fd) {
		if (fclose(fd) != 0)
			LOG_MSG(LOG__ERROR, true, "close()");
	}

	return ret;
}
/**/

ssize_t get_default_code(code_type_e code_type, char **code)
{
	ssize_t ret = -1;

	static char def_routing_code[PATH_MAX] = {0};
	static char def_life_code[PATH_MAX] = {0};
	static char def_prio_code[PATH_MAX] = {0};


	switch (code_type) {
	case ROUTING_CODE:
		if (*def_routing_code == '\0') {
			if (ini_gets("executor", "def_routing_code", NULL, def_routing_code, PATH_MAX, world.shm->config_file) == 0) {
				LOG_MSG(LOG__ERROR, false, "Default routing code (def_routing_code) is not specified in the config file %s", world.shm->config_file);
				goto end;
			}
		}
		ret = load_code_from_file(def_routing_code, code);
		break;
	case LIFE_CODE:
		if (*def_life_code == '\0') {
			if (ini_gets("executor", "def_life_code", NULL, def_life_code, PATH_MAX, world.shm->config_file) == 0) {
				LOG_MSG(LOG__ERROR, false, "Default routing code (def_life_code) is not specified in the config file %s", world.shm->config_file);
				goto end;
			}
		}
		ret = load_code_from_file(def_life_code, code);
		break;

	case PRIO_CODE:
		if (*def_prio_code ==  '\0') {
			if (ini_gets("executor", "def_prio_code", NULL, def_prio_code, PATH_MAX, world.shm->config_file) == 0) {
				LOG_MSG(LOG__ERROR, false, "Default routing code (def_prio_code) is not specified in the config file %s", world.shm->config_file);
				goto end;
			}
		}
		ret = load_code_from_file(def_prio_code, code);
		break;

	}
end:

	return ret;
}

int get_code_and_info(code_type_e code_type, const char *bundle_id, /*out*/char **code, /*out*/int *code_l, /*out*/bundle_info_s *info)
{
	int ret = 1, use_default = 1;
	uint8_t *bundle_raw = NULL;
	mmeb_body_s *mmeb = NULL;

	if (open_raw_bundle(world.bundles_path, bundle_id, &bundle_raw) != 0) {
		LOG_MSG(LOG__ERROR, false, "Can't load bundle %s", bundle_id);
		goto end;
	}

	mmeb = calloc(1, sizeof(mmeb_body_s));
	if (get_bundle_content(bundle_raw, info, mmeb) != 0) {
		LOG_MSG(LOG__ERROR, false, "Can't find mmeb extension, using default codes.");
	} else {
		if (find_code(mmeb, code_type, code, code_l) != 0) {
			LOG_MSG(LOG__ERROR, false, "Can't find code, using default codes.");
		} else {
			use_default = 0;
		}
	}
	bundle_free_mmeb(mmeb);

	if (use_default) {
		LOG_MSG(LOG__INFO, false, "Can't find code for bundle %s, using default code.", bundle_id);
		if ((*code_l = get_default_code(code_type, code)) == -1) {
			LOG_MSG(LOG__ERROR, false, "Error loading default code");
			*code_l = 0;
			goto end;
		}
	}

	ret = 0;
end:
	if (bundle_raw)
		free(bundle_raw);

	return ret;
}