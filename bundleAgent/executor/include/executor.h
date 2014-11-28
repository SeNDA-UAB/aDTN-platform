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

#include <limits.h> //NAME_MAX

#include "modules/exec_c_helpers/include/adtnrhelper.h"
#include "modules/include/hash.h"

#include "common/include/bundle.h"
#include "common/include/executor.h"
#include "common/include/constants.h"

#define DEF_SOCKNAME "executor"

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
