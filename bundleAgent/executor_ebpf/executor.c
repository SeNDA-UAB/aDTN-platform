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
#include <signal.h>
#include <pthread.h>
#include <stdint.h> 
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include "ebpf/exec.h"

#include "common/include/shm.h"
#include "common/include/log.h"
#include "common/include/init.h"
#include "common/include/bundle.h"
#include "common/include/executor.h"
#include "common/include/utils.h"
#include "common/include/minIni.h"

#define DEF_SOCKNAME "executor"

typedef struct _world {
	char *bundles_path;
	char *objects_path;
	struct common *shm;
} world_s;

world_s world;

int initialize_socket(char *sockname)
{
	int ret = 0;
	int s = 0;
	struct sockaddr_un addr = {0};
	struct stat stat_file = {0};

	if ((s = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
		LOG_MSG(LOG__ERROR, true, "socket()");
		ret = -1;
		goto end;
	}

	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sockname, sizeof(addr.sun_path));

	//If socket exists, delete it
	if (stat(sockname, &stat_file) == 0) {
		unlink(sockname);
	}

	if ((ret = bind(s, (struct sockaddr *) &addr, sizeof(struct sockaddr_un))) == -1) {
		LOG_MSG(LOG__ERROR, true, "bind()");
		goto end;
	}
end:
	if (ret == -1) {
		close(s);
		unlink(sockname);
	} else {
		ret = s;
	}

	return ret;
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

ssize_t get_default_code(code_type_e code_type, char **code)
{
	ssize_t ret = -1;

	static char def_forwarding_code[PATH_MAX] = {0};
	static char def_life_code[PATH_MAX] = {0};
	static char def_priority_code[PATH_MAX] = {0};
	static char def_delivery_code[PATH_MAX] = {0};

	switch (code_type) {
	case ROUTING_CODE:
		if (*def_forwarding_code == '\0') {
			if (ini_gets("executor", "def_forwarding_code", "", def_forwarding_code, PATH_MAX, world.shm->config_file) == 0) {
				LOG_MSG(LOG__ERROR, false, "Default routing code (def_forwarding_code) is not specified in the config file %s", world.shm->config_file);
				goto end;
			}
		}
		ret = load_code_from_file(def_forwarding_code, code);
		break;

	case LIFE_CODE:
		if (*def_life_code == '\0') {
			if (ini_gets("executor", "def_life_code", "", def_life_code, PATH_MAX, world.shm->config_file) == 0) {
				LOG_MSG(LOG__ERROR, false, "Default routing code (def_life_code) is not specified in the config file %s", world.shm->config_file);
				goto end;
			}
		}
		ret = load_code_from_file(def_life_code, code);
		break;

	case PRIO_CODE:
		if (*def_priority_code ==  '\0') {
			if (ini_gets("executor", "def_priority_code", "", def_priority_code, PATH_MAX, world.shm->config_file) == 0) {
				LOG_MSG(LOG__ERROR, false, "Default routing code (def_priority_code) is not specified in the config file %s", world.shm->config_file);
				goto end;
			}
		}
		ret = load_code_from_file(def_priority_code, code);
		break;

	case DELIVERY_CODE:
		if (*def_delivery_code ==  '\0') {
			if (ini_gets("executor", "def_delivery_code", "", def_delivery_code, PATH_MAX, world.shm->config_file) == 0) {
				LOG_MSG(LOG__ERROR, false, "Default routing code (def_delivery_code) is not specified in the config file %s", world.shm->config_file);
				goto end;
			}
		}
		ret = load_code_from_file(def_delivery_code, code);
		break;

    default:
        break;
	}
end:

	return ret;
}

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

static int get_bundle_content(const uint8_t *bundle_raw, /*out*/char **dest, /*out*/mmeb_body_s *mmeb)
{
	int ret = 0;

    if (bundle_get_destination(bundle_raw, (uint8_t **)dest) != 0) {
        LOG_MSG(LOG__ERROR, false, "Can't get destination.");
        ret = 1;
        goto end;
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

int get_code_and_info(code_type_e code_type, const char *bundle_id, /*out*/char **code, /*out*/int *code_l, /*out*/ char **dest)
{
	int ret = 1, use_default = 1;
	uint8_t *bundle_raw = NULL;
	mmeb_body_s *mmeb = NULL;

	if (open_raw_bundle(world.bundles_path, bundle_id, &bundle_raw) != 0) {
		LOG_MSG(LOG__ERROR, false, "Can't load bundle %s", bundle_id);
		goto end;
	}

	mmeb = calloc(1, sizeof(mmeb_body_s));
	if (get_bundle_content(bundle_raw, dest, mmeb) != 0) {
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


union _response *process_exec_petition(struct _petition *p)
{
	int mmeb_present = 1;
    char *dest;
	union _response *r = malloc(sizeof(union _response));
    uint8_t *bundle_raw = NULL;
	mmeb_body_s *mmeb = NULL;

    r->simple.header = p->header;
	
    if (open_raw_bundle(world.bundles_path, p->header.bundle_id, &bundle_raw) != 0) {
		LOG_MSG(LOG__ERROR, false, "Can't load bundle %s", p->header.bundle_id);
		goto end;
	}

	if (get_bundle_content(bundle_raw, &dest, mmeb) != 0) {
		LOG_MSG(LOG__ERROR, false, "Can't find mmeb extension, using default codes.");
	    mmeb_present = 0;
    }

    switch(p->header.code_type) {
        case ROUTING_CODE:
            {
                char *routing_code, *code_state = NULL;
                int routing_code_l, code_state_l = 0;

                if (mmeb_present) {
                    if (find_code(mmeb, ROUTING_CODE, &routing_code, &routing_code_l) != 0) {
                        routing_code_l = get_default_code(ROUTING_CODE, &routing_code);
                    }
                    find_code(mmeb, CODE_STATE, &code_state, &code_state_l);
                }

                int result = execute_code(routing_code, routing_code_l, code_state);

                if (result == 0) {
                    routing_exec_result *exec_r = getForwardDests();
                    r->exec_r.correct = 1;
                    r->exec_r.num_hops = exec_r->hops_list_l;
                    memcpy(r->exec_r.hops_list, exec_r->hops_list, 
                            sizeof(r->exec_r.hops_list));
                } else {
                    r->exec_r.correct = 0;
                }

                break;
            }
        case DELIVERY_CODE:
            {
                char *delivery_code, *code_state = NULL;
                int delivery_code_l, code_state_l = 0;

                if (mmeb_present) {
                    if (find_code(mmeb, DELIVERY_CODE, &delivery_code, &delivery_code_l) != 0) {
                        delivery_code_l = get_default_code(DELIVERY_CODE, &delivery_code);
                    }
                    find_code(mmeb, CODE_STATE, &code_state, &code_state_l);
                }

                int result = execute_code(delivery_code, delivery_code_l, code_state);
                if (result == 0) {
                    r->simple.correct = 1;
                    r->simple.result = result;

                    // TODO: Update CODE_STATE

                } else {
                    r->simple.correct = 0;
                }

                break;
            }
        case PRIO_CODE:
            {
                char *prio_code; 
                int prio_code_l;

                if (mmeb_present) {
                    if (find_code(mmeb, DELIVERY_CODE, &prio_code, &prio_code_l) != 0) {
                        prio_code_l = get_default_code(DELIVERY_CODE, &prio_code);
                    }
                }

                int result = execute_code(prio_code, prio_code_l, NULL);
                if (result == 0) {
                    r->simple.correct = 1;
                    r->simple.result = result;
                } else {
                    r->simple.correct = 0;
                }

                break;
            }
        case LIFE_CODE:
            {
                char *life_code; 
                int life_code_l;

                if (mmeb_present) {
                    if (find_code(mmeb, DELIVERY_CODE, &life_code, &life_code_l) != 0) {
                        life_code_l = get_default_code(DELIVERY_CODE, &life_code);
                    }
                }

                int result = execute_code(life_code, life_code_l, NULL);
                if (result == 0) {
                    r->simple.correct = 1;
                    r->simple.result = result;
                } else {
                    r->simple.correct = 0;
                }

                break;
            }
        default:
            break;
    }

end:
    return r;
}

int main(int argc, char *argv[])
{
	int ret = 0, main_socket;
	size_t sockname_l;
	char *sockname;

	//Init aDTN process (shm and global option)
	if (init_adtn_process(argc, argv, &world.shm) != 0) {
		LOG_MSG(LOG__ERROR, false, "Process initialization failed. Aborting execution");
		ret = 1;
		goto end;
	}

    // Get bundles path
	int bundles_path_l = strlen(world.shm->data_path) + strlen(QUEUE_PATH) + 2;
	world.bundles_path = (char *)malloc(bundles_path_l);
	snprintf(world.bundles_path, bundles_path_l, "%s/%s", world.shm->data_path, QUEUE_PATH);

	// Initialize petitions socket
	sockname_l = strlen(world.shm->data_path) + strlen(DEF_SOCKNAME) + 7;
	sockname = (char *) malloc(sockname_l);
	snprintf(sockname, sockname_l, "%s/%s.sock", world.shm->data_path, DEF_SOCKNAME);

	main_socket = initialize_socket(sockname);
	if (main_socket == -1) {
		LOG_MSG(LOG__ERROR, true, "Socket initialization failed");
		ret = 1;
		goto end;
	}

	// Wait for petitions
	fd_set readfds, safe;
	FD_ZERO(&readfds);
	FD_SET(main_socket, &readfds);
	safe = readfds;

    union _response *r;
	while (select (FD_SETSIZE, &readfds, NULL, NULL, NULL)) {
		if (FD_ISSET(main_socket, &readfds)) {
			struct _petition p;
			ssize_t p_l;
			struct sockaddr_un src_addr = {0};
			socklen_t src_addr_l = sizeof(src_addr);

            // Receive petition
			p_l = recvfrom(main_socket, &p, sizeof(p), 0, (struct sockaddr *)&src_addr, (socklen_t *)&src_addr_l);

			if (p_l != sizeof(p))
				continue;

			switch (p.header.petition_type) {
				case EXE:
					{
                        r = process_exec_petition(&p);
						break;
					}
				case RM:
					/* Ignored */
					break;
				default:
					/* Ignored */
					break;
			}

            // Send response
            sendto(main_socket, &r, sizeof(r), 0, (struct sockaddr *)&src_addr, (socklen_t)sizeof(src_addr));
        }
	}

end:
	return ret;
}
