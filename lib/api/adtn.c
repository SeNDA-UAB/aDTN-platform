#include "common/include/uthash.h"
#include "common/include/bundle.h"
#include "common/include/shm.h"
#include "common/include/config.h"
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
} bunsock_s;

/* VARIABLES */

bunsock_s *sockStore;

/**
    Creates a adtn sockets to send or recv information using the adtn platform.

    aDTN is divided in two parts, one is the API for developers that allow to use functions to send and receive messages. The other one
    is the core that manages and sends the messages.

    ====================
    |      Dev API     |
    ====================
    |      aDTN Core   |
    ====================

    The Core makes posible resilience to delay, disruptions and big taxes of errors. The implementation of the paltform allow more than one core to cohexist in
    the same node(device).

    ==========================
    |         Dev API        |
    ==========================
    |  aDTN Core  | aDTN Core|
    ==========================

    Config_file allow to swpecify the platform associated with the socket.

    @return a descriptor identifiyng the socket on succes or a value equal or minor to 0 on error.
*/
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
		identifier = (bunsock_s *)malloc(sizeof(bunsock_s));
		identifier->id = sock_id;
		identifier->opt.rcode = NULL;
		identifier->opt.r_fromfile = 0;
		identifier->opt.lcode = NULL;
		identifier->opt.l_fromfile = 0;
		identifier->opt.pcode = NULL;
		identifier->opt.p_fromfile = 0;
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

/**
    When adtn socket is created with adtn_socket() it exist in the space name but has no address associated. adtn_bind(2) associates the information
    specified in the structure sock_addr_t of the second argument to the socket represented by the file descriptor.

    @param fd The file descriptor that identifies the adtn_socket. This value can be obtained calling adtn_socket()
    @param address A sock_addr_t structure containing information about the address that must be associated with the socket

    @return 0 on succes, diferent from 0 otherwise. If the function return a value different from 0 the variable errno is set.

    errno can take the values below:

    ENOTSOCK    the file descriptor is not a valid adtn_socket descriptor.
    EACCES      the program hasn't enough permisions to create bind the socket
*/
int adtn_var_socket(socket_params in)
{
	char default_config_file[CONFIG_FILE_PATH_L] = {0};
	char *config_file;
	int ret = -1;
	char *data_path = NULL;

	if (strcmp(PREFIX, "/usr") == 0)
		snprintf(default_config_file, CONFIG_FILE_PATH_L, "/etc/adtn/adtn.ini");
	else
		snprintf(default_config_file, CONFIG_FILE_PATH_L, "%s/etc/adtn/adtn.ini", PREFIX);

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

int adtn_bind(int fd, sock_addr_t *address)
{
	int ret = 1;
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
	address->id = address->id ? address->id : shm->platform_id;
	f = fopen(SOCK_SPOOL, "r");
	if (f != NULL) {
		while (EOF != (fscanf(f, "%*[^\n]"), fscanf(f, "%*c")))
			++lines;
		rewind(f);
		while (j < lines) {
			fscanf(f, "%s %d", idport, &pid);
			platform_id = strtok_r(idport, ":", &strk_state);
			adtn_port = atoi(strtok_r(NULL, ":", &strk_state));
			if ((adtn_port == address->adtn_port) && (strcmp(address->id, platform_id) == 0)) {
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
	fprintf(f, "%s:%d %d\n", address->id, address->adtn_port, getpid());
	identifier->addr.id = strdup(address->id);
	identifier->addr.adtn_port = address->adtn_port;
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

/**
    Closes an adtn_socket and frees the memory associated to the socket. The structures associated to the socket
    in adtn_bind(2) call are freed too.

    @param fd A file descriptor identifying the socket.

    @return 0 on succes, non 0 otherwise. If the function fails errno is set.

    ENOTSOCK    the file descriptor is not a valid adtn_socket descriptor.
    EACCES      the program hasn't enough permisions to eliminate the socket
*/
int adtn_close(int fd)
{
	int ret = 1, i = 0;
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

/**
    Closes an adtn_socket and frees the memory associated to the socket. The structures associated to the socket
    in adtn_bind(2) call are freed too. If some data are waiting to be readed it will be deleted.

    @param fd A file descriptor identifying the socket.

    @return 0 on succes, non 0 otherwise. If the function fails errno is set.

    ENOTSOCK    the file descriptor is not a valid adtn_socket descriptor.
    EACCES      the program hasn't enough permisions to eliminate the socket
*/
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

/**
    Over ADTN platform the messages can carry codes with him to perform different tasks. [REF]
    This function allows to add code options to the socket. Each code linked with the socket will be added to outcoming messages sent through the socket.
    The kind of code associated to the socket is specified by the parameter option_name.

    @param fd A file descriptor identifying the socket.
    @param option_name determines what kind of code will be associated to the socket. R_CODE will add a routing code. This code will be executed in each hop of
    the network to determine the next hop. L_CODE will add a life time code. This code will be executed in each node of the network to decide if the message is
    lapsed or not. P_CODE will add a prioritzation code. This code will affect only to the messages in other nodes that pertain to same application. Is not possible
    for a code to priorize messages from other applications.
    @param code This parameter must contain a filename where the code is stored or the full code to associate to socket. The content of this parameter is associated
    with parameter from_file
    @param from_file [Optional] If is set to 1 the param code will contain a filename where the routing code is stored. If is set to 0 the param code must contain all
    code. Default value is 0.
    @param replace [Optional] If is set to 1 and exists an older code associated to the socket it will be replaced. If is set to 0 and a code is already binded with
    the socket this function will return an error. Replace is 0 by default

    @return 0 if the code is correctly associated to the socket or non 0 otherwise. If the function fails errno is set

    ENOTSOCK    the file descriptor is not a valid adtn_socket descriptor.
*/
int adtn_setcodopt_base(int fd, code_type_e option, const char *code, int from_file, int replace)
{
	int ret = 1;
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

/**
    Allows to remove an associated code from a socket. See documentation of adtn_setcodopt(4) for more information.

    @param fd A file descriptor identifying the socket
    @param option_name determines what kind of code will be removed. If the code doesn't exist nothing happens.

    @return 0 if the code is succefully removed of not exist or non 0 otherwise. If the function fails errno is set.

    ENOTSOCK    the file descriptor is not a valid adtn_socket descriptor.
*/
int adtn_rmcodopt(int fd, int option)
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

/**
    In ADTN messages is posible specify the lifetime of the message and diferents options for headers.
    These options can be associated to a socket using this function. After calling it, all messages send through the socket will ahve the specified
    options.

    @param fd A file descriptor identifying the socket
    @param options a sock_opt_t structure containing the options that must be associated to the socket

    @return 0 on succes or non 0 otherwise
*/
int adtn_setsockopt(int fd, sock_opt_t options)
{
	int ret = 1;
	bunsock_s *identifier;
	HASH_FIND_INT( sockStore, &fd, identifier );
	if (identifier == NULL) {
		errno = ENOTSOCK;
		goto end;
	}
	identifier->sopt.lifetime = (options.lifetime > 1) ? options.lifetime : identifier->sopt.lifetime;
	identifier->sopt.proc_flags = options.proc_flags ? options.proc_flags : H_DESS | H_NOTF;
	identifier->sopt.block_flags = options.block_flags ? options.block_flags : B_DEL_NP;

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

static int delegate_bundle(const bundle_s *bundle, const char *file_prefix)
{
	int ret = 1;
	int len;
	int bundle_raw_l;
	uint8_t *bundle_raw = NULL;
	char *full_path = NULL;
	char bundle_name[BUNDLE_NAME_LENGTH] = {0};
	struct timeval curr_t;
	FILE *f = NULL;
	char *msg = NULL;

	gettimeofday(&curr_t, NULL);
	snprintf(bundle_name, BUNDLE_NAME_LENGTH, "%ld-%ld-A.bundle", curr_t.tv_sec, curr_t.tv_usec);

	len = strlen(file_prefix) + 1 + strlen(RECEIVER_DIR) + 1 + strlen(bundle_name) + 1;
	full_path = calloc(len, sizeof(char));
	snprintf(full_path, len, "%s/"RECEIVER_DIR"/%s", file_prefix, bundle_name);

	if ((bundle_raw_l = bundle_create_raw(bundle, &bundle_raw)) <= 0) {
		goto end;
	}

	printf("Printing bundle %s\nSize of bundle: %d\n", full_path, bundle_raw_l);
	bundle_get_printable(bundle, &msg);
	printf("%s\n", msg);
	if (msg) free(msg);

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

/**
    Sends a message over ADTN platform. The fact that adtn_sendto call returns a success value doesn't mean that the message has been sent through the network.
    adtn_sendto() only puts the message in queue to be sent. The platform will manage the send through the network. In opportunistic networks the time
    until adtn_sendto() returns a succes value and the message is really send is impossible to determine.

    @param fd A file descriptor identifying the socket
    @param addr sock_addr_t structure containing the destination information. Must contain, the application port and ip of the destination.
    @param buffer A buffer with the message to send

    @return -1 on error or the number of bytes written on succes.
*/
int adtn_sendto(int fd, const sock_addr_t addr, char *buffer)
{
	int ret = 1;
	int shm_fd = -1;
	char *first_oc;
	char full_dest[ENDPOINT_LENGTH] = {0};
	char full_src[ENDPOINT_LENGTH] = {0};
	struct common *shm;
	bunsock_s *identifier;
	bundle_s *bundle = NULL;
	payload_block_s *payload;

	if (addr.id == NULL || strcmp(addr.id, "") == 0 || buffer == NULL || strcmp(buffer, "") == 0) {
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
	bundle_set_destination(bundle, full_dest);
	bundle_set_source(bundle, full_src);
	bundle_set_proc_flags(bundle, identifier->sopt.proc_flags);
	bundle_set_lifetime(bundle, identifier->sopt.lifetime);
	payload = bundle_new_payload_block();
	bundle_set_payload(payload, (uint8_t *) buffer, strlen(buffer));
	bundle_add_ext_block(bundle, (ext_block_s *) payload);
	if (bundle_add_codes(bundle, identifier) != 0) {
		goto end;
	}
	if (delegate_bundle(bundle, shm->data_path) != 0) {
		goto end;
	}
	ret = 0;
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
	if (mkdir (dir_path, 0755) == -1 && errno != EEXIST)
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

/**
    Recive a message from adtn_socket. If at the moment of perform the call there are no messages adtn_recv(3) will block
    until can retrieve a message. After get a message it will be deleted from the socket queue.

    @param fd A file descriptor identifying the socket.
    @param buffer Char array where the mssg value will be stored
    @param max_len The maximum number of bytes that will be written into buffer

    @return The number of bytes received on succes or -1 on error. In case of error errno is set.

    ENOTSOCK    the file descriptor is not a valid adtn_socket descriptor.
*/
int adtn_recv(int fd, char *buffer, int max_len)
{
	unsigned len = 0;
	int ret = -1;
	char *raw_data = NULL;
	uint8_t *payload;

	if (!buffer || max_len < 1) {
		errno = EINVAL;
		goto end;
	}
	raw_data = adtn_recv_base(fd);
	if (!raw_data)
		goto end;
	len = bundle_raw_get_payload((uint8_t *)raw_data, &payload);
	if (len > max_len)
		len =  max_len;
	memcpy(buffer, (char *)payload, len);
	ret = len;
end:
	if (raw_data)
		free(raw_data);

	return ret;
}

/**
    Recive a message from adtn_socket. If at the moment of perform the call there are no messages adtn_recvfrom(4) will block
    until can retrieve a message. After get a message it will be deleted from the socket queue. A sock_addr_t struct will be fill with
    sender information like ip or port.

    @param fd A file descriptor identifying the socket.
    @param buffer Char array where the mssg value will be stored
    @param max_len The maximum number of bytes that will be written into buffer
    @param addr An empty structure that will be filled with information about the sender

    @return The number of bytes received on succes or -1 on error. In case of error errno is set.

    ENOTSOCK    the file descriptor is not a valid adtn_socket descriptor.
*/
int adtn_recvfrom(int fd, char *buffer, int max_len, sock_addr_t *addr)
{
	int ret = 1;
	int len;
	char *raw_data = NULL;
	char *full_src = NULL;
	char *strk_state;
	uint8_t *payload;

	if (!buffer || max_len < 1) {
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

	len = bundle_raw_get_payload((uint8_t *)raw_data, &payload);
	if (len > max_len)
		len =  max_len;
	memcpy(buffer, (char *)payload, len);
	ret = len;

end:
	if (raw_data)
		free(raw_data);

	return ret;
}
