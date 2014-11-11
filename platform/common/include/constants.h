#ifndef INC_COMMON_CONSTANTS
#define INC_COMMON_CONSTANTS


/*
    Common constants
*/
#define STRING_IP_LEN 16
#define STRING_IP_PORT_LEN 22
#define MAX_PLATFORM_ID_LEN 255
#define PING_APP 1

/*
    Bundle constants
*/
#define MAX_SDNV_LENGTH 8 // Bytes
#define MAX_ID_LEN 255
#define MIN_ID_LEN 1
#define MAX_BUNDLE_SIZE 6291456
#define RFC_DATE_2000 946684800
#define MAX_BUNDLE_MSG 2048

/*
    Executor constants
*/
#define MAX_HOPS_LEN 1024
#define POOL_SIZE 1
#define BUF_SIZE 255
#define FWK 1

/*
    Log constants
*/
#define MAX_LOG_MSG 512     //In bytes
#define DEBUG 1
#ifndef NOLOG
#define NOLOG 0
#endif

#define ERROR_LOG_FILE "adtn.err.log"
#define INFO_LOG_FILE "adtn.info.log"
#define MAX_ENAME 133

/*
    Neighbours constants
*/
#define MAX_NB_ID_LEN 255
//${ANNOUNCED_INFO}/${nb_id}/subscribed/
#define RIT_SUBSCRIBED_PATH ANNOUNCED_INFO"/%s/subscribed"

/*
    Path
*/
#ifndef PREFIX
#define PREFIX "/usr/local"
#endif
#ifndef SYSCONFDIR
#define SYSCONFDIR PREFIX "/etc"
#endif
#ifndef CONF_FILE_NAME
#define CONF_FILE_NAME "adtn.ini"
#endif
#ifndef DEFAULT_CONF_FILE_PATH
#define DEFAULT_CONF_FILE_PATH SYSCONFDIR"/"ADTN_CONF_SUBDIR"/"CONF_FILE_NAME
#endif
#ifndef BIN_PREFIX
#define BIN_PREFIX PREFIX"/bin"
#endif
#ifndef LIB_PREFIX
#define LIB_PREFIX PREFIX"/lib"
#endif

//Data paths (relative to data path from adtn.ini)
#define QUEUE_PATH "bundle_queue"
#define OBJECTS_PATH QUEUE_PATH"/codes"   //relative to spool_path
#define INPUT_PATH "input"
#define DEST_PATH "incoming"


/*
    RIT Paths and RIT constants
*/
#define MAX_DESTS 1000

#define THIS_NODE "/thisNode"
#define BUNDLES THIS_NODE"/bundles"

#define GPS_BRANCH_LATITUDE THIS_NODE"/GPS/latitude"
#define GPS_BRANCH_LONGITUDE THIS_NODE"/GPS/longitude"
#define GPS_BRANCH_ALTITUDE THIS_NODE"/GPS/altitude"
#define GPS_BRANCH_TIME THIS_NODE"/GPS/timestamp"

#define HABITAT_BRANCH THIS_NODE"/hlra/habitat"
#define HABITAT_CHAR_BRANCH THIS_NODE"/hlra/habitatChar"

#define NBS_BRANCH "/nbs"
#define NBS_HABITAT_CHAR_ENC "habitatCharEnc"

// Format BUNDLES_DESTS/{0..999}/{lat,lon,alt}
#define BUNDLES_DESTS BUNDLES"/dests"
#define BUNDLE_DESTS_NUM BUNDLES_DESTS"/num"
#define NBS_INFO "/nbs"
#define ANNOUNCED_INFO "/announced"
#define LOCAL_INFO "/local"
#define MAX_RIT_PATH 2048

#define SEPARATOR "/"
#define ROOT "/"

/*
    SHM Constants
*/
#define SHM_MEM "adtn"
#define SHM_ID_LENGTH 11   //Length of crc32 hexadecimal number

/*
    Queue Constants
*/
#define SOCK_PATH "/queue_manager.sock" //change it
#define MAX_BUFFER 256
#define CONN_TIMEOUT 2000000 //in usecs

/*
    Stats Constants
*/
#define SHM_PATH "/dev/shm/"
#define BASE_PATH "/adtn_stats"
#define PERMISSIONS S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH

/*
    Processor Constants
*/
#define WAIT_EMPTY_QUEUE 1000000/10 //time in usecs
#define WAIT_FULL_QUEUE 1000000/10 //time in usecs
#define NB_TIMEOUT 1200000 //time in usecs
#define MIN_PORT 1
#define MAX_PORT 65535

/*
    Receiver Constants
*/
#define IN_SOCK_TIMEOUT 10
#define SUBSC_PATH "%s/subscribed"

/*
    QueueManager Constants
*/
#define EXEC_TIMEOUT 1000000 //This time is in usecs
#define PROC_TIMEOUT 3000000 //Time in usec
#define SCHEDULER_TIMER 1000000

#endif