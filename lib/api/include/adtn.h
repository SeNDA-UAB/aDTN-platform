#include <errno.h>
#include <stdint.h>

typedef struct {
	int adtn_port; ///< port to identify the own application
	// sem_t *semaphore; ///< semaphore to add bundles to queue
	char *id; ///< IP addres that is associated with the socket.
} sock_addr_t;

typedef struct {
	uint32_t lifetime; ///< lifetime of bundle
	uint32_t proc_flags; ///< proc flags for main header
	uint32_t block_flags; ///< proc flags for extension blocks
} sock_opt_t;

typedef struct {
	int fd;
	int option_name; ///< algorithm-> code_type_e defined in bundle.h
	const char *code;
	int from_file;
	int replace;
} set_opt_args;

typedef struct {
	const char *config_file;
} socket_params;

/* MACROS */
#define adtn_setcodopt(...) adtn_var_setcodopt((set_opt_args){__VA_ARGS__});
#define adtn_socket(...) adtn_var_socket((socket_params){__VA_ARGS__});

/* FUNCTIONS */

//int adtn_socket();
int adtn_var_socket(socket_params in);
int adtn_bind(int fd, sock_addr_t *addr);
int adtn_close(int fd);
int adtn_shutdown(int fd);

//int adtn_setcodopt(set_opt_args in);
int adtn_rmcodopt(int fd, int option_name);
int adtn_setsockopt(int fd, sock_opt_t options);
//int adtn_addsockdata(int fd, const char* key, const char* value);
//int adtn_rmsockdata(int fd, const char* key);

int adtn_sendto(int fd, sock_addr_t addr, char *buffer);
int adtn_recv(int fd, char *buffer, int max_len);
int adtn_recvfrom(int fd, char *buffer, int max_len, sock_addr_t *addr);
