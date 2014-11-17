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
	char *report;
	char *custom;
	char *dest;
	char *source;
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

typedef struct {
	uint64_t last_timestamp;
} sock_get_t;

/* Option codes */
#define OP_PROC_FLAGS 1
#define OP_LIFETIME 2
#define OP_BLOCK_FLAGS 3
#define OP_DEST 4
#define OP_SOURCE 5
#define OP_REPORT 6
#define OP_CUSTOM 7
#define OP_LAST_TIMESTAMP 8

/* MACROS */
#define adtn_setcodopt(...) adtn_var_setcodopt((set_opt_args){__VA_ARGS__});
#define adtn_socket(...) adtn_var_socket((socket_params){__VA_ARGS__});

/* FUNCTIONS */

int adtn_var_socket(socket_params in);
int adtn_bind(int fd, sock_addr_t *addr);
int adtn_close(int fd);
int adtn_shutdown(int fd);

int adtn_rmcodopt(int fd, int option_name);
//int adtn_setsockopt(int fd, sock_opt_t options);
int adtn_setsockopt(int fd, int optname, const void *optval);
int adtn_getsockopt(int fd, int optname, void *optval, int *optlen);

int adtn_sendto(int fd, sock_addr_t addr, char *buffer);
int adtn_recv(int fd, char *buffer, int max_len);
int adtn_recvfrom(int fd, char *buffer, int max_len, sock_addr_t *addr);
