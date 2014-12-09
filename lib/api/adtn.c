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

#include "common/include/uthash.h"
#include "common/include/bundle.h"
#include "common/include/shm.h"
#include "common/include/config.h"
#include "common/include/utils.h"
#include "common/include/constants.h"
#include "include/adtn.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <stdbool.h>
//#include <stdarg.h>

//TODO: check defines
#define ID_LENGTH 120
#define ENDPOINT_LENGTH 36
#define BUNDLE_NAME_LENGTH 120
#define PATH_LENGTH 312
#define CONFIG_FILE_PATH_L 256
#define SeNDA_FRAMEWORK 0x01

#define RECEIVER_DIR "input"
#define SOCK_SPOOL "/dev/shm/adtn_sockets"
#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )
#define INOTIFY_EVENT_SIZE  ( sizeof (struct inotify_event) + NAME_MAX + 1)
#define INOTIFY_EVENT_BUF (1*INOTIFY_EVENT_SIZE)


/* DATA STRUCTURES */
typedef struct {
	char *rcode; ///< Routing code. Can be the full code or  a filename where the code is stored.
	int r_fromfile; ///< Specifies if rcode contatins full code or a filename

	char *lcode; ///< Life time code.
	int l_fromfile;

	char *pcode; ///< prioritzation Code
	int p_fromfile;
} cod_opt_t;

typedef struct {
	char *key;
	char *value;
	UT_hash_handle hh;
} bundle_data_s;

typedef struct {
	int id; ///< Bundle sock identifier
	char *data_path; ///< Full path for config file, selects the platform associated to the socket
	sock_addr_t addr; ///< structure with info about the IP and the application
	cod_opt_t opt; ///< structure with code options
	sock_opt_t sopt; ///< structure with header options
	bundle_data_s *bdata; ///< data
	UT_hash_handle hh;
	sock_get_t gval;  ///< Last bundle send values
} bunsock_s;

/* VARIABLES */

bunsock_s *sockStore;

static int adtn_socket_base(const char *data_path)
{
	static int UID = 1;
	int sock_id = -1;
	int len;
	bunsock_s *identifier;

	++UID;
	HASH_FIND_INT( sockStore, &sock_id, identifier);
	if (identifier == NULL) {
		sock_id = UID;
		identifier = (bunsock_s *)calloc(1, sizeof(bunsock_s));
		identifier->id = sock_id;
		identifier->sopt.proc_flags = H_DESS | H_NOTF;
		identifier->sopt.block_flags = B_DEL_NP;
		identifier->sopt.lifetime = 100000; //this time is in seconds
		len = strlen(data_path) + 1;
		identifier->data_path = (char *)calloc(len, sizeof(char));
		strncpy(identifier->data_path, data_path, len);
		identifier->bdata = NULL;

		HASH_ADD_INT( sockStore, id, identifier);
	} else {
		errno = EBUSY;
	}

	return sock_id;
}

int adtn_var_socket(socket_params in)
{
	char default_config_file[CONFIG_FILE_PATH_L] = {0};
	char *config_file;
	int ret = -1;
	char *data_path = NULL;

	snprintf(default_config_file, CONFIG_FILE_PATH_L, DEFAULT_CONF_FILE_PATH);
	config_file = in.config_file ? strdup(in.config_file) : strdup(default_config_file);

	struct conf_list ritm_configuration = {0};
	if (load_config("global", &ritm_configuration, config_file) != 1) {
		errno = ENOENT;
		goto end;
	}
	if ((data_path = get_option_value("data", &ritm_configuration)) == NULL) {
		errno = EBADRQC;
		goto end;
	}
	ret = 0;
end:
	if (ritm_configuration.section)
		free(ritm_configuration.section);
	free(config_file);
	ret = (ret == -1) ? -1 : adtn_socket_base(data_path);
	if (data_path)
		free(data_path);

	return ret;
}

int adtn_bind(int fd, sock_addr_t *addr)
{
	int ret = -1;
	int i = 0, j = 0, lines = 0;
	int adtn_port, pid;
	int shm_fd = -1;
	char idport[ID_LENGTH + 6] = {0}; //ID_LENGTH+":"(1) + port (5)
	char *strk_state, *platform_id;
	bunsock_s *identifier;
	FILE *f = NULL, *src = NULL, *dest = NULL;
	struct common *shm;

	HASH_FIND_INT(sockStore, &fd, identifier);
	if (!identifier) {
		errno = ENOTSOCK;
		goto end;
	}
	if (load_shared_memory_from_path(identifier->data_path, &shm, &shm_fd, false) != 0) {
		errno = ENOENT;
		goto end;
	}
	addr->id = addr->id ? addr->id : shm->platform_id;
	f = fopen(SOCK_SPOOL, "r");
	if (f != NULL) {
		while (EOF != (fscanf(f, "%*[^\n]"), fscanf(f, "%*c")))
			++lines;
		rewind(f);
		while (j < lines) {
			fscanf(f, "%s %d", idport, &pid);
			platform_id = strtok_r(idport, ":", &strk_state);
			adtn_port = atoi(strtok_r(NULL, ":", &strk_state));
			if ((adtn_port == addr->adtn_port) && (strcmp(addr->id, platform_id) == 0)) {
				kill(pid, 0);
				if (errno != ESRCH) {
					errno = EADDRINUSE;
					goto close_f;
				}
				src = fopen(SOCK_SPOOL, "r");
				dest = fopen(SOCK_SPOOL".tmp", "w");
				if (!src || !dest) {
					errno = EACCES;
					goto close_f;
				}
				while (i < lines) {
					memset(idport, 0, ID_LENGTH + 6);
					fscanf(src, "%s %d", idport, &pid);
					if (i != j)
						fprintf(dest, "%s %d\n", idport, pid);
					++i;
				}
				fclose(src);
				src = NULL;
				fclose(dest);
				dest = NULL;
				remove(SOCK_SPOOL);
				rename(SOCK_SPOOL".tmp", SOCK_SPOOL);
				break;
			}
			++j;
		}
		fclose(f);
		f = NULL;
	}

	f = fopen(SOCK_SPOOL, "a");
	if (!f) {
		errno = EACCES;
		goto close_f;
	}
	fprintf(f, "%s:%d %d\n", addr->id, addr->adtn_port, getpid());
	identifier->addr.id = strdup(addr->id);
	identifier->addr.adtn_port = addr->adtn_port;
	ret = 0;

close_f:
	if (shm_fd != -1)
		close(shm_fd);
	if (f)
		fclose(f);
	if (dest)
		fclose(dest);
	if (src)
		fclose(src);
end:

	return ret;
}

int adtn_close(int fd)
{
	int ret = -1, i = 0;
	int adtn_port, pid;
	int _pid;
	FILE *src = NULL, *dest = NULL;
	char idport[ID_LENGTH + 6] = {0}; //ID_LENGTH+":"(1) + port (5)
	char *strk_state, * platform_id = NULL;
	bunsock_s *identifier = NULL;

	HASH_FIND_INT(sockStore, &fd, identifier);
	if (!identifier) {
		errno = ENOTSOCK;
		goto end;
	}
	_pid = getpid();
	src = fopen(SOCK_SPOOL, "r");
	dest = fopen(SOCK_SPOOL".tmp", "w");
	if (!src || !dest) {
		errno = EACCES;
		goto end;
	}

	memset(idport, 0, ID_LENGTH + 6);
	while (EOF != fscanf(src, "%s %d", idport, &pid)) {
		platform_id = strtok_r(idport, ":", &strk_state);
		adtn_port = atoi(strtok_r(NULL, ":", &strk_state));
		if (!((adtn_port == identifier->addr.adtn_port) && (strcmp(identifier->addr.id, platform_id) == 0) && (pid == _pid)) )
			fprintf(dest, "%s:%d %d\n", platform_id, adtn_port, pid);
		memset(idport, 0, ID_LENGTH + 6);
		++i;
	}
	fclose(src);
	fclose(dest);
	src = NULL;
	dest = NULL;
	remove(SOCK_SPOOL);
	rename(SOCK_SPOOL".tmp", SOCK_SPOOL);
	free(identifier->addr.id);
	free(identifier->data_path);
	HASH_DEL(sockStore, identifier);
	free(identifier);

	ret = 0;
end:
	if (src)
		fclose(src);
	if (dest)
		fclose(dest);

	return ret;
}

int adtn_shutdown(int fd)
{
	bunsock_s *identifier;
	DIR *dir;
	int len, ret = 1;
	int shm_fd = -1;
	struct dirent *file;
	struct common *shm;
	char *destination_path = NULL;
	char *file_path = NULL;

	HASH_FIND_INT(sockStore, &fd, identifier);
	if (!identifier) {
		errno = ENOTSOCK;
		goto end;
	}
	if (load_shared_memory_from_path(identifier->data_path, &shm, &shm_fd, false) != 0) {
		errno = ENOENT;
		goto end;
	}
	len = strlen(shm->data_path) + strlen("/incoming/") + 6 + 1;
	destination_path = calloc(len, 1);
	snprintf(destination_path, len, "%s/incoming/%d", shm->data_path, identifier->addr.adtn_port);
	dir = opendir(destination_path);
	if (!dir) {
		ret = 0;
		goto end;
	}
	while ((file = readdir(dir))) {
		if (file->d_type != 8)
			continue;
		if (file_path) free(file_path);
		len = strlen(destination_path) + 1 + strlen(file->d_name) + 1;
		file_path = calloc(len, 1);
		snprintf(file_path, len, "%s/%s", destination_path, file->d_name);
		remove(file_path);
	}
	rmdir(destination_path);
	if (adtn_close(fd))
		goto end;
	ret = 0;
end:
	if (shm_fd != -1)
		close(shm_fd);
	if (dir)
		closedir(dir);
	if (file_path)
		free(file_path);
	if (destination_path)
		free(destination_path);

	return ret;
}

int adtn_setcodopt_base(int fd, code_type_e option, const char *code, int from_file, int replace)
{
	int ret = -1;
	bunsock_s *identifier;

	HASH_FIND_INT(sockStore, &fd, identifier);
	if (!identifier) {
		errno = ENOTSOCK;
		goto end;
	}
	if (code == NULL || strcmp(code, "") == 0 || from_file < 0 || from_file > 1) {
		errno = EINVAL;
		goto end;
	}
	switch (option) {
	case ROUTING_CODE:
		if (identifier->opt.rcode) {
			if (replace) {
				free(identifier->opt.rcode);
			} else {
				errno = EOPNOTSUPP;
				goto end;
			}
		}
		identifier->opt.rcode = strdup(code);
		identifier->opt.r_fromfile = from_file;
		break;
	case LIFE_CODE:
		if (identifier->opt.lcode) {
			if (replace) {
				free(identifier->opt.lcode);
			} else {
				errno = EOPNOTSUPP;
				goto end;
			}
		}
		identifier->opt.lcode = strdup(code);
		identifier->opt.l_fromfile = from_file;
		break;
	case PRIO_CODE:
		if (identifier->opt.pcode) {
			if (replace) {
				free(identifier->opt.pcode);
			} else {
				errno = EOPNOTSUPP;
				goto end;
			}
		}
		identifier->opt.pcode = strdup(code);
		identifier->opt.p_fromfile = from_file;
		break;
	default:
		errno = EINVAL;
		goto end;
	}
	ret = 0;
end:

	return ret;
}

int adtn_var_setcodopt(set_opt_args in)
{
	int ret;
	int from_file_out;
	int replace_out;

	errno = 0;
	if (!in.fd)
		errno = EINVAL;
	if (!in.code)
		errno = EINVAL;
	from_file_out = in.from_file ? in.from_file : 0;
	replace_out = in.replace ? in.replace : 0;

	ret = (errno == 0) ? adtn_setcodopt_base(in.fd, in.option_name, in.code, from_file_out, replace_out) : 1;

	return ret;
}

int adtn_rmcodopt(int fd, const int option)
{
	int ret = 1;
	bunsock_s *identifier;

	HASH_FIND_INT( sockStore, &fd, identifier);
	if (!identifier) {
		errno = ENOTSOCK;
		goto end;
	}
	switch (option) {
	case ROUTING_CODE:
		if (identifier->opt.rcode) {
			free(identifier->opt.rcode);
			identifier->opt.rcode = NULL;
			identifier->opt.r_fromfile = 0;
		}
		break;
	case LIFE_CODE:
		if (identifier->opt.lcode) {
			free(identifier->opt.lcode);
			identifier->opt.lcode = NULL;
			identifier->opt.l_fromfile = 0;
		}
		break;
	case PRIO_CODE:
		if (identifier->opt.pcode) {
			free(identifier->opt.pcode);
			identifier->opt.pcode = NULL;
			identifier->opt.p_fromfile = 0;
		}
		break;
	default:
		errno = EINVAL;
		goto end;
	}

	ret = 0;
end:

	return ret;
}

int adtn_setsockopt(int fd, const int optname, const void *optval)
{
	int ret = -1;
	bunsock_s *identifier;
	HASH_FIND_INT( sockStore, &fd, identifier );
	if (identifier == NULL) {
		errno = ENOTSOCK;
		goto end;
	}
	switch (optname) {
	case OP_PROC_FLAGS:
		identifier->sopt.proc_flags = *(uint32_t *)optval;
		break;
	case OP_LIFETIME:
		identifier->sopt.lifetime = *(uint32_t *)optval;
		break;
	case OP_BLOCK_FLAGS:
		identifier->sopt.block_flags = *(uint32_t *)optval;
		break;
	case OP_DEST:
		if (identifier->sopt.dest != NULL)
			free(identifier->sopt.dest);
		identifier->sopt.dest = strdup((char *)optval);
		break;
	case OP_SOURCE:
		if (identifier->sopt.source != NULL)
			free(identifier->sopt.source);
		identifier->sopt.source = strdup((char *)optval);
		break;
	case OP_REPORT:
		if (identifier->sopt.report != NULL)
			free(identifier->sopt.report);
		identifier->sopt.report = strdup((char *)optval);
		break;
	case OP_CUSTOM:
		if (identifier->sopt.custom != NULL)
			free(identifier->sopt.custom);
		identifier->sopt.custom = strdup((char *)optval);
		break;
	default:
		errno = ENOTSUP;
		goto end;
	}
	ret = 0;
end:

	return ret;
}

int adtn_getsockopt(int fd, const int optname,  void *optval, int *optlen)
{
	int ret = -1;
	bunsock_s *identifier;
	HASH_FIND_INT( sockStore, &fd, identifier );
	if (identifier == NULL) {
		errno = ENOTSOCK;
		goto end;
	}
	switch (optname) {
	case OP_PROC_FLAGS:
		if (*optlen >= sizeof(identifier->sopt.proc_flags)) {
			*(uint32_t *)optval = (identifier->sopt.proc_flags);
			*optlen = sizeof(identifier->sopt.proc_flags);
		} else {
			*optlen = 0;
		}
		break;
	case OP_LIFETIME:
		if (*optlen >= sizeof(identifier->sopt.lifetime)) {
			*(uint32_t *)optval = (identifier->sopt.lifetime);
			*optlen = sizeof(identifier->sopt.lifetime);
		} else {
			*optlen = 0;
		}
		break;
	case OP_BLOCK_FLAGS:
		if (*optlen >= sizeof(identifier->sopt.block_flags)) {
			*(uint32_t *)optval = (identifier->sopt.block_flags);
			*optlen = sizeof(identifier->sopt.block_flags);
		} else {
			*optlen = 0;
		}
		break;
	case OP_DEST:
		*optlen = snprintf((char *)optval, *optlen, "%s", identifier->sopt.dest);
		break;
	case OP_SOURCE:
		*optlen = snprintf((char *)optval, *optlen, "%s", identifier->sopt.source);
		break;
	case OP_REPORT:
		*optlen = snprintf((char *)optval, *optlen, "%s", identifier->sopt.report);
		break;
	case OP_CUSTOM:
		*optlen = snprintf((char *)optval, *optlen, "%s", identifier->sopt.custom);
		break;
	case OP_LAST_TIMESTAMP:
		if (*optlen >= sizeof(identifier->gval.last_timestamp)) {
			*(uint64_t *)optval = (identifier->gval.last_timestamp);
			*optlen = sizeof(identifier->gval.last_timestamp);
		} else {
			*optlen = 0;
		}
		break;
	default:
		errno = ENOTSUP;
		goto end;
	}
	ret = 0;
end:

	return ret;
}

static char *read_all_from_file(char *filename)
{
	int n_bytes;
	int r_bytes;
	char *code = NULL;
	FILE *f = NULL;

	f = fopen(filename, "r");
	if (!f) {
		goto end;
	}
	fseek(f, 0L, SEEK_END);
	n_bytes = ftell(f);
	rewind(f);
	if (n_bytes <= 0) {
		errno = EINVAL;
		goto end;
	}
	code = (char *)calloc(n_bytes, sizeof(char));
	r_bytes = fread(code, sizeof(char), n_bytes, f);
	if (r_bytes < n_bytes) {
		errno = EINVAL;
		goto end;
	}
end:
	if (f)
		fclose(f);

	return code;
}

static int bundle_add_codes(bundle_s *bundle, bunsock_s *identifier)
{
	int must_add = 0;
	int ret = 1;
	mmeb_s *mmeb;
	char *code = NULL;

	mmeb = bundle_new_mmeb(1);
	if (identifier->opt.rcode) {
		code = identifier->opt.r_fromfile ? read_all_from_file(identifier->opt.rcode) : strdup(identifier->opt.rcode);
		if (!code)
			goto end;
		bundle_add_mmeb_code(mmeb, ROUTING_CODE, SeNDA_FRAMEWORK, strlen(code), (uint8_t *)code);
		++must_add;
	}
	if (identifier->opt.lcode) {
		code = identifier->opt.l_fromfile ? read_all_from_file(identifier->opt.lcode) : strdup(identifier->opt.lcode);
		if (!code)
			goto end;
		bundle_add_mmeb_code(mmeb, LIFE_CODE, SeNDA_FRAMEWORK, strlen(code), (uint8_t *)code);
		++must_add;
	}
	if (identifier->opt.pcode) {
		code = identifier->opt.p_fromfile ? read_all_from_file(identifier->opt.pcode) : strdup(identifier->opt.pcode);
		if (!code)
			goto end;
		bundle_add_mmeb_code(mmeb, PRIO_CODE, SeNDA_FRAMEWORK, strlen(code), (uint8_t *)code);
		++must_add;
	}
	if (must_add > 0) {
		bundle_set_ext_block_flags((ext_block_s *)mmeb, identifier->sopt.block_flags);
		bundle_add_ext_block(bundle, (ext_block_s *)mmeb);
	}
	if (code)
		free(code);
	ret = 0;
end:

	return ret;
}

static int delegate_bundle(const char *bundle_name, const bundle_s *bundle, const char *file_prefix, bunsock_s *identifier)
{
	int ret = 1;
	int len;
	int bundle_raw_l;
	uint8_t *bundle_raw = NULL;
	char *full_path = NULL;
	FILE *f = NULL;

	len = strlen(file_prefix) + 1 + strlen(RECEIVER_DIR) + 1 + strlen(bundle_name) + 1;
	full_path = calloc(len, sizeof(char));
	snprintf(full_path, len, "%s/"RECEIVER_DIR"/%s", file_prefix, bundle_name);

	if ((bundle_raw_l = bundle_create_raw(bundle, &bundle_raw)) <= 0) {
		goto end;
	}
	if (bundle_raw_check(bundle_raw, bundle_raw_l) != 0) {
		errno = EMSGSIZE;
		goto end;
	}
	identifier->gval.last_timestamp = bundle->primary->timestamp_time;

	f = fopen(full_path, "wb");
	if (!f)
		goto end;
	fwrite(bundle_raw, 1, bundle_raw_l, f);
	ret = 0;
end:
	if (f)
		fclose(f);
	if (full_path)
		free(full_path);
	if (bundle_raw)
		free(bundle_raw);

	return ret;
}

int adtn_sendto(int fd, const void *buffer, size_t buffer_l, const sock_addr_t addr)
{
	int ret = -1;
	int shm_fd = -1;
	char *first_oc, *bundle_name;
	char full_dest[ENDPOINT_LENGTH] = {0};
	char full_src[ENDPOINT_LENGTH] = {0};
	struct common *shm;
	bunsock_s *identifier;
	bundle_s *bundle = NULL;
	payload_block_s *payload;

	if (addr.id == NULL || strcmp(addr.id, "") == 0 || buffer_l <= 0 || (buffer_l > 0 && buffer == NULL)) {
		errno = EINVAL;
		goto end;
	}
	HASH_FIND_INT( sockStore, &fd, identifier);
	if (!identifier) {
		errno = ENOTSOCK;
		goto end;
	}
	if (load_shared_memory_from_path(identifier->data_path, &shm, &shm_fd, false) != 0) {
		errno = ENOENT;
		goto end;
	}

	first_oc = strchr(addr.id, ':');
	if (!first_oc) {
		snprintf(full_dest, ENDPOINT_LENGTH - 1, "%s:%d", addr.id, addr.adtn_port);
	} else {
		if ( (strlen(first_oc + 1) < 0) || (strlen(first_oc + 1) > 6) || (strtol(first_oc + 1, NULL, 10) <= 0)) {
			errno = EINVAL;
			goto end;
		}
		snprintf(full_dest, ENDPOINT_LENGTH - 1, "%s:%d", addr.id, addr.adtn_port);
	}
	snprintf(full_src, ENDPOINT_LENGTH - 1, "%s:%d", identifier->addr.id, identifier->addr.adtn_port);

	bundle = bundle_new();
	if (identifier->sopt.dest) {
		bundle_set_destination(bundle, identifier->sopt.dest);
	} else {
		bundle_set_destination(bundle, full_dest);
	}
	if (identifier->sopt.source) {
		bundle_set_source(bundle, identifier->sopt.source);
	} else {
		bundle_set_source(bundle, full_src);
	}
	bundle_set_proc_flags(bundle, identifier->sopt.proc_flags);
	bundle_set_lifetime(bundle, identifier->sopt.lifetime);
	if (identifier->sopt.report)
		bundle_set_report(bundle, identifier->sopt.report);
	if (identifier->sopt.custom)
		bundle_set_custom(bundle, identifier->sopt.custom);
	payload = bundle_new_payload_block();
	bundle_set_payload(payload, (uint8_t *) buffer, buffer_l);
	bundle_add_ext_block(bundle, (ext_block_s *) payload);
	if (bundle_add_codes(bundle, identifier) != 0) {
		goto end;
	}

	bundle_name = generate_bundle_name(shm->platform_id);
	if (delegate_bundle(bundle_name, bundle, shm->data_path, identifier) != 0) {
		goto end;
	}
	free(bundle_name);

	ret = buffer_l;
end:
	if (shm_fd != -1)
		close(fd);
	if (bundle)
		bundle_free(bundle);

	return ret;
}

static char *adtn_recv_base(int fd)
{
	int len, wd;
	int ifd = -1;
	int received = 0;
	int shm_fd = -1;
	char *dir_path = NULL;
	char full_file_path[PATH_LENGTH] = {0};

	char *p = NULL;
	char buffer[INOTIFY_EVENT_BUF];
	int buffer_l;
	struct inotify_event *event = NULL;

	char *raw_data = NULL;
	struct common *shm;
	bunsock_s *identifier;
	DIR *dir;

	HASH_FIND_INT(sockStore, &fd, identifier);
	if (!identifier) {
		errno = ENOTSOCK;
		goto end;
	}
	if (load_shared_memory_from_path(identifier->data_path, &shm, &shm_fd, false) != 0) {
		errno = ENOENT;
		goto end;
	}

	len = strlen(shm->data_path) + 1 + strlen("incoming") + 1 + 8 + 1; //8 comes from maximum adtn port identifier
	dir_path = (char *)calloc(len, sizeof(char));
	snprintf(dir_path, len, "%s/incoming/%d", shm->data_path, identifier->addr.adtn_port);
	if (mkdir (dir_path, 0777) == -1 && errno != EEXIST)
		goto end;
	ifd = inotify_init();
	if (ifd < 0) {
		goto end;
	}
	wd = inotify_add_watch(ifd, dir_path, IN_CLOSE_WRITE);
	if (wd == -1) {
		goto end;
	}

	// Look for bundles already in the folder
	dir = opendir(dir_path);
	if (!dir)
		goto end;

	struct dirent *file;
	while (1) {
		file = readdir(dir);
		if (!file)
			break;
		if (file->d_type != 8)
			continue;
		memset(full_file_path, 0, PATH_LENGTH);
		snprintf( full_file_path, PATH_LENGTH - 1, "%s/%s", dir_path, file->d_name);
		do {
			raw_data = read_all_from_file(full_file_path);
		} while (raw_data == NULL);
		remove(full_file_path); //Remove readed bundles
		received = 1;
		break;
	}
	closedir(dir);

	if (!received) {
		// Wait for new bundles
		buffer_l = read(ifd, buffer, INOTIFY_EVENT_BUF);
		if (buffer_l < 0)
			goto end;
		for (p = buffer; p < buffer + buffer_l; ) {
			event = (struct inotify_event *) p;
			memset(full_file_path, 0, PATH_LENGTH);
			snprintf( full_file_path, PATH_LENGTH - 1, "%s/%s", dir_path, event->name);
			do {
				raw_data = read_all_from_file(full_file_path);
			} while (raw_data == NULL);
			remove(full_file_path); //Remove readed bundles
			break;
		}
	}
end:
	if (ifd != -1)
		close(ifd);
	if (shm_fd != -1)
		close(shm_fd);
	if (dir_path)
		free(dir_path);

	return raw_data;
}

int adtn_recv(int fd, void *buffer, size_t len)
{
	unsigned payload_len = 0;
	int ret = -1;
	char *raw_data = NULL;
	uint8_t *payload;

	if (!buffer || len < 1) {
		errno = EINVAL;
		goto end;
	}
	raw_data = adtn_recv_base(fd);
	if (!raw_data)
		goto end;
	payload_len = bundle_raw_get_payload((uint8_t *)raw_data, &payload);
	if (payload_len > len)
		payload_len =  len;
	memcpy(buffer, (char *)payload, payload_len);
	ret = payload_len;
end:
	if (raw_data)
		free(raw_data);

	return ret;
}

int adtn_recvfrom(int fd, void *buffer, size_t len, sock_addr_t *addr)
{
	int ret = -1;
	int payload_len;
	char *raw_data = NULL;
	char *full_src = NULL;
	char *strk_state;
	uint8_t *payload;

	if (!buffer || len < 1) {
		errno = EINVAL;
		goto end;
	}
	raw_data = adtn_recv_base(fd);
	if (!raw_data)
		goto end;

	if (bundle_raw_get_primary_field((uint8_t *)raw_data, SOURCE_SCHEME, &full_src) != 0) {
		goto end;
	}
	addr->id = strtok_r(full_src, ":", &strk_state);
	addr->adtn_port = atoi(strtok_r(NULL, ":", &strk_state));

	payload_len = bundle_raw_get_payload((uint8_t *)raw_data, &payload);
	if (payload_len > len)
		payload_len =  len;
	memcpy(buffer, (char *)payload, payload_len);
	ret = payload_len;

end:
	if (raw_data)
		free(raw_data);

	return ret;
}
