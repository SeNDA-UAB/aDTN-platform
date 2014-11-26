#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include "common/include/constants.h"
/** @file */
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

int adtn_rmcodopt(int fd, const int option);
int adtn_var_setcodopt(set_opt_args in);

int adtn_setsockopt(int fd, const int optname, const void *optval);
int adtn_getsockopt(int fd, const int optname,  void *optval, int *optlen);

int adtn_sendto(int fd, const void *buffer, size_t buffer_l, const sock_addr_t addr);
int adtn_recv(int fd, void *buffer, size_t len);
int adtn_recvfrom(int fd, void *buffer, size_t len, sock_addr_t *addr);


/**
    @fn int adtn_socket(socket_params in)
    @brief Creates a adtn sockets to send or recv information using the adtn platform.

    aDTN is divided in two parts, one is the API for developers that allow to use functions to send and receive messages. The other one
    is the core that manages and sends the messages.
    ====================
    |      Dev API     |
    ====================
    |      aDTN Core   |
    ====================
    The Core makes posible resilience to delay, disruptions and big taxes of errors. The implementation of the platform allow more than one core to cohexist in
    the same node(device).
    ==========================
    |         Dev API        |
    ==========================
    |  aDTN Core  | aDTN Core|
    ==========================

    @param in A socket_params structure containing the configuration file path.

    @return a descriptor identifiyng the socket on succes, -1 on error. If the function fails errno is set.

    errno can take the values below:

    EBUSY       Cannot allocate socket.
    ENOENT      global configuration missing in configuration file.
    EBADRQC     data field missing in configuration file.
*/

/**
    @fn int adtn_bind(int fd, sock_addr_t *address)
    @brief Associates the information given to the socket.

    When adtn socket is created with adtn_socket(1) it exist in the space name but has no address associated. adtn_bind(2) associates the information
    specified in the structure sock_addr_t of the second argument to the socket represented by the file descriptor.

    @param fd The file descriptor that identifies the adtn_socket. This value can be obtained calling adtn_socket(1).
    @param address A sock_addr_t structure containing information about the address that must be associated with the socket.

    @return 0 on succes, -1 on error. If the function fails errno is set.

    errno can take the values below:

    ENOTSOCK    the file descriptor is not a valid adtn_socket descriptor.
    ENOENT      cannot load the shared memory.
    EACCES      the program hasn't enough permissions to create bind the socket.
    EADDRINUSE  the address is already in use.
*/

/**
    @fn int adtn_close(int fd)
    @brief Closes an adtn_socket and frees the memory associated to the socket.

    Closes an adtn_socket and frees the memory associated to the socket. The structures associated to the socket
    in adtn_bind(2) call are freed too.

    @param fd A file descriptor identifying the socket.

    @return 0 on succes, -1 on error. If the function fails errno is set.

    errno can take the values below:

    ENOTSOCK    the file descriptor is not a valid adtn_socket descriptor.
    EACCES      the program hasn't enough permisions to eliminate the socket.
*/

/**
    @fn int adtn_shutdown(int fd)
    @brief Same as adtn_close(1), deleting also waiting data.

    Closes an adtn_socket and frees the memory associated to the socket. The structures associated to the socket
    in adtn_bind(2) call are freed too. If some data are waiting to be readed it will be deleted.

    @param fd A file descriptor identifying the socket.

    @return 0 on succes, -1 on error. If the function fails errno is set.

    errno can take the values below:

    ENOTSOCK    the file descriptor is not a valid adtn_socket descriptor.
    ENOENT      cannot load the shared memory.
    EACCES      the program hasn't enough permisions to eliminate the socket
*/

/**
    @fn int adtn_setcodopt(set_opt_args in)
    @brief Associate codes to a socket.

    Over ADTN platform the messages can carry codes with him to perform different tasks.
    This function allows to add code options to the socket. Each code linked with the socket will be added to outcoming messages sent through the socket.
    The kind of code associated to the socket is specified by the parameter option_name.

    @param fd A file descriptor identifying the socket.
    @param option_name determines what kind of code will be associated to the socket. R_CODE will add a routing code. This code will be executed in each hop of
    the network to determine the next hop. L_CODE will add a life time code. This code will be executed in each node of the network to decide if the message is
    lapsed or not. P_CODE will add a prioritzation code. This code will affect only to the messages in other nodes that pertain to same application. Is not possible
    for a code to priorize messages from other applications.
    @param code This parameter must contain a filename where the code is stored or the full code to associate to socket. The content of this parameter is associated
    with parameter from_file.
    @param from_file [Optional] If is set to 1 the param code will contain a filename where the routing code is stored. If is set to 0 the param code must contain all
    code. Default value is 0.
    @param replace [Optional] If is set to 1 and exists an older code associated to the socket it will be replaced. If is set to 0 and a code is already binded with
    the socket this function will return an error. Replace is 0 by default.

    @return 0 on succes, -1 on error. If the function fails errno is set.

    errno can take the values below:

    ENOTSOCK    the file descriptor is not a valid adtn_socket descriptor.
    EINVAL      invalid code or from_file value.
    EOPNOTSUPP  existing code binded.
*/

/**
    @fn int adtn_rmcodopt(int fd, int option)
    @brief Removes an associated code from a socket.

    Allows to remove an associated code from a socket. See documentation of adtn_setcodopt(1) for more information.

    @param fd A file descriptor identifying the socket.
    @param option_name determines what kind of code will be removed. If the code doesn't exist nothing happens.

    @return 0 on succes, -1 on error. If the function fails errno is set.

    errno can take the values below:

    ENOTSOCK    the file descriptor is not a valid adtn_socket descriptor.
    EINVAL      invalid code option.
*/

/**
   @fn int adtn_setsockopt(int fd, int optname, const void *optval)
   @brief Set message options.

   In ADTN messages is posible specify the lifetime of the message and diferents options for headers.
   These options can be associated to a socket using this function. After calling it, all messages send through the socket will ahve the specified
   options.

   @param fd A file descriptor identifying the socket.
   @param optname determines the option to set, the possible values are listed below.
   @param optval the value to set.

   @return 0 on succes, -1 on error. If the function fails errno is set.

Options available:

   OP_PROC_FLAGS       Set the flags for the primary block.
   OP_LIFETIME         Set the lifetime of the messages.
   OP_BLOCK_FLAGS      Set the flags for the rest of blocks.
   OP_DEST             Set the destination of the messages.
   OP_SOURCE           Set the source of the message.
   OP_REPORT           Set the report of the message.
   OP_CUSTOM           Set the custom value of the message.

errno can take the values below:

ENOTSOCK    the file descriptor is not a valid adtn_socket descriptor.
ENOTSUP     invalid option.
*/

/**
   @fn int adtn_getsockopt(int fd, int optname, void *optval, int *optlen)
   @brief Get message options.

   @param fd A file descriptor identifying the socket.
   @param optname determines the option to set, the possible values are listed below.
   @param optval where to save the value obtined.
   @param optlen number of bytes to get, after calling is set to the number of bytes returned.

   @return 0 on succes, -1 on error. If the function fails errno is set.

Options available:

   OP_PROC_FLAGS       Get the flags for the primary block.
   OP_LIFETIME         Get the lifetime of the messages.
   OP_BLOCK_FLAGS      Get the flags for the rest of blocks.
   OP_DEST             Get the destination of the messages.
   OP_SOURCE           Get the source of the message.
   OP_REPORT           Get the report of the message.
   OP_CUSTOM           Get the custom value of the message.
   OP_LAST_TIMESTAMP   Get the last message timestamp.

errno can take the values below:

ENOTSOCK    the file descriptor is not a valid adtn_socket descriptor.
ENOTSUP     invalid option.
*/

/**
    @fn int adtn_sendto(int fd, const void *buffer, size_t buffer_l, const sock_addr_t addr)
    @brief Sends a message over ADTN platform.

    Sends a message over ADTN platform. The fact that adtn_sendto call returns a success value doesn't mean that the message has been sent through the network.
    adtn_sendto(3) only puts the message in queue to be sent. The platform will manage the send through the network. In opportunistic networks the time
    until adtn_sendto(3) returns a succes value and the message is really send is impossible to determine.

    @param fd A file descriptor identifying the socket.
    @param buffer A buffer with the message to send.
    @param buffer_l the length to send of  the buffer.
    @param addr sock_addr_t structure containing the destination information. Must contain, the application port and ip of the destination.
    
    @return The number of bytes written on succes or -1 on error. If the function fails errno is set.

    errno can take the values below:

    EINVAL      invalid address or null buffer.
    ENOTSOCK    the file descriptor is not a valid adtn_socket descriptor.
    ENOENT      cannot load the shared memory.
    EMSGSIZE    message size is too big.

*/

/**
    @fn int adtn_recv(int fd, void *buffer, size_t len)
    @brief Recive a message.

    Recive a message from adtn_socket. If at the moment of perform the call there are no messages adtn_recv(3) will block
    until can retrieve a message. After get a message it will be deleted from the socket queue.

    @param fd A file descriptor identifying the socket.
    @param buffer array where the mssg value will be stored.
    @param len The maximum number of bytes that will be written into buffer.

    @return The number of bytes received on succes or -1 on error. If the function fails errno is set.

    errno can take the values below:

    EINVAL      invalid buffer or len.
    ENOTSOCK    the file descriptor is not a valid adtn_socket descriptor.
    ENOENT      cannot load the shared memory.

*/

/**
    @fn int adtn_recvfrom(int fd, void *buffer, size_t len, sock_addr_t *addr)
    @brief Recive a message, filling sender information.

    Recive a message from adtn_socket. If at the moment of perform the call there are no messages adtn_recvfrom(4) will block
    until can retrieve a message. After get a message it will be deleted from the socket queue. A sock_addr_t struct will be fill with
    sender information like ip or port.

    @param fd A file descriptor identifying the socket.
    @param buffer array where the mssg value will be stored.
    @param len The maximum number of bytes that will be written into buffer.
    @param addr An empty structure that will be filled with information about the sender.

    @return The number of bytes received on succes or -1 on error. If the function fails errno is set.

    errno can take the values below:

    EINVAL      invalid buffer or len.
    ENOTSOCK    the file descriptor is not a valid adtn_socket descriptor.
    ENOENT      cannot load the shared memory.
*/

