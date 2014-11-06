#include <limits.h> //NAME_MAX

#include "modules/exec_c_helpers/include/adtnrhelper.h"
#include "modules/include/hash.h"

#include "common/include/bundle.h"
#include "common/include/executor.h"

#define DEF_SOCKNAME "executor"
#define POOL_SIZE 1
#define BUF_SIZE 255
#define FWK 1

#ifndef DEBUG
	#define DEBUG 1
#endif


typedef struct _worker_params {
	int thread_num;
	int main_socket;
	uint8_t *respawn_child;
	pthread_mutex_t **respawn_child_mutex;

	pthread_mutex_t *preparing_exec;
} worker_params;

enum exec_state {
	RECV_PETITION,
	LOAD_CODE,
	RM_BUNDLE,
	RM_OK,
	RM_ERROR,
	NOTIFY_CHILD_RESPAWN_AND_RESPAWN,
	RESPAWN_CHILD,
	EXEC_CODE,
	EXEC_ERROR,
	EXEC_OK,
	SEND_RESULT,
	END
};

struct _child_exec_petition {
	char bundle_id[NAME_MAX];
	code_type_e code_type;

	/* code_type == ROUTING */
	char dest[MAX_ID_LEN];
	routing_dl_s *routing_dl;
	/**/

	/* code_type == LIFETIME || code_type == PRIO*/
	prio_dl_s *prio_dl;
	life_dl_s *life_dl;
	/**/
};

/**/
