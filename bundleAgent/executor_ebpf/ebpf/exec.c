#include <string.h>
#include <stdlib.h>

#include "vm/inc/ubpf.h"
#include "vm/ebpf.h"
#include "vm/ubpf_int.h"

#include "exec.h"

#include "common/include/constants.h"
#include "common/include/rit.h"

#define MAX_STATE_VARS 128

static char *bundle_state[MAX_STATE_VARS];
static nbs_iter_info nbs_info = {
	.nbs_list = NULL,
	.nbs_list_l = 0,
	.position = 0
};

static routing_exec_result *r_result;
static int deliver = 0;
static int forward = 0;
static int remove = 0;


/* Execution context */
void setup_state(char *code_state) 
{
	/* Format: state1;state2;(..)\0 */
	char *saveptr = code_state;
	char *next_state = strtok(code_state, ";");
	int i = 0;
	while (next_state != NULL) {
		bundle_state[i] = next_state;	
		++i;
	}
}

char *getState(const int state_num) 
{
	return bundle_state[state_num];
}

void setState(const int state_num, char *state)
{
	bundle_state[state_num] = strdup(state);
}

char *ritGet(const char *rit_path) 
{
	return rit_getValue(rit_path);
}

void ritSet(const char *rit_path, const char *value)
{
	rit_set(rit_path, value);
}

void deliverMsg()
{
	deliver = 1;
}

void forwardMsg()
{
	forward = 1;
}

void removeMsg()
{
	remove = 1;
}

void addHop(const char *hop)
{
	if (hop != NULL && *hop != '\0') {
		r_result->hops_list = realloc(r_result->hops_list, (r_result->hops_list_l + 1) * MAX_PLATFORM_ID_LEN * sizeof(char));
		strncpy(r_result->hops_list + (r_result->hops_list_l)*MAX_PLATFORM_ID_LEN, hop, MAX_PLATFORM_ID_LEN);
		r_result->hops_list_l++;
	}
}

static char *restartNB()
{
	nbs_info.position = 0;
	return nbs_info.nbs_list + nbs_info.position * MAX_PLATFORM_ID_LEN;
}

static int hasNextNB()
{
	if (nbs_info.position == nbs_info.nbs_list_l)
		return 0;
	else
		return 1;
}

static char *NB(int i)
{
	if (i > nbs_info.nbs_list_l)
		return "";
	else
		return nbs_info.nbs_list + (i * MAX_PLATFORM_ID_LEN);
}

static char *nextNB()
{
	if (nbs_info.position == nbs_info.nbs_list_l)
		return "";
	else
		return nbs_info.nbs_list + (nbs_info.position++*MAX_PLATFORM_ID_LEN);
}

static int numNBs()
{
	return nbs_info.nbs_list_l;
}
/**/

void setNbsInfo(nbs_iter_info nbs)
{
	nbs_info = nbs;
}

int getDeliver() 
{
	return deliver;
}

int getForward()
{
	return forward;
}

int getRemove()
{
	return remove;
}

routing_exec_result *getForwardDests()
{
	return r_result;
}

struct ubpf_vm *prepareEBPFVM()
{
	// Create ubpf vm
    struct ubpf_vm *vm = ubpf_create();
    if (!vm) {
        return NULL;
    }

	// Register functions
	ubpf_register(vm, 1, "getState", (void *) getState);
	ubpf_register(vm, 2, "setState", (void *) setState);
	ubpf_register(vm, 3, "ritGet", (void *) ritGet);
	ubpf_register(vm, 4, "ritSet", (void *) ritSet);
	ubpf_register(vm, 5, "deliverMsg", (void *) deliverMsg);
	ubpf_register(vm, 6, "forwardMsg", (void *) forwardMsg);
	ubpf_register(vm, 7, "removeMsg", (void *) removeMsg);
	ubpf_register(vm, 8, "addHop", (void *) addHop);
	ubpf_register(vm, 9, "restartNB", (void *) restartNB);
	ubpf_register(vm, 10, "hasNextNB", (void *) hasNextNB);
	ubpf_register(vm, 11, "NB", (void *) NB);
	ubpf_register(vm, 12, "nextNB", (void *) nextNB);
	ubpf_register(vm, 13, "numNBs", (void *) numNBs);

	return vm;
}

int execute_code(char *code, int code_l, char *code_state) 
{
	int ret = 1;

	// Prepare vm
	struct ubpf_vm *vm = prepareEBPFVM();
	if (vm == NULL)
		goto end;

	// Prepare state
	if (code_state)
		setup_state(code_state);
	
	// Copy code to vm
	vm->insts = (struct ebpf_inst *) malloc(code_l);
	memcpy((void *) vm->insts, code, code_l);
	vm->num_insts = code_l / 8;

	// Compile code
	char *errmsg;
	ubpf_jit_fn fn = ubpf_compile(vm, &errmsg);
    if (!fn) {
		ubpf_destroy(vm);
		goto end;	
    }

	// Execute delivery code
	ret = fn(NULL, 0);

end:
	return ret;
}
