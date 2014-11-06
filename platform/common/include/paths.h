/**
	Prefix that can be used by all functions.
**/

#ifndef PREFIX
#define PREFIX "/usr/local"
#endif
#ifndef ETC_PREFIX
#define ETC_PREFIX "/etc/adtn/"
#endif
#ifndef CONF_FILE_NAME
#define CONF_FILE_NAME "adtn.ini"
#endif
#ifndef DEFAULT_CONF_FILE
#define DEFAULT_CONF_FILE ETC_PREFIX""CONF_FILE_NAME
#endif
#ifndef BIN_PREFIX
#define BIN_PREFIX PREFIX"/bin"
#endif
#ifndef LIB_PREFIX
#define LIB_PREFIX PREFIX"/lib"
#endif

//Data paths
#define FIFO_FILE "adtn.pipe"
#define BUNDLE_QUEUE_SEM "queue_sem"
#define SPOOL_PATH "/bundle_queue/"
//#define SPOOL_QUEUE_PATH SPOOL_PATH"/queue"
#define OBJECTS_PATH SPOOL_PATH"codes/"   //relative to spool_path
#define INPUT_PATH "/input/"
#define DEST_PATH "/incoming/"
