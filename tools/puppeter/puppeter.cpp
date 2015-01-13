/*
	TODO:
		* Obtenir args.
		* Convertir testEnv a una classe.


 */

#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
#endif

//Dyninst
#include "BPatch.h"
#include "BPatch_addressSpace.h"
#include "BPatch_process.h"
#include "BPatch_binaryEdit.h"
#include "BPatch_function.h"
#include "BPatch_point.h"
#include "BPatch_flowGraph.h"

//C
#include <linux/limits.h> // PATH_MAX

//CPP
#include <string>

/* Configuration */
#define DYNINSTAPI_RT_LIB "/usr/local/lib64/libdyninstAPI_RT.so"

#define NUM_PROCS 5

enum bundleAgentProc_e {
	INFORMATION_EXCHANGE,
	QUEUEMANAGER,
	PROCESSOR,
	RECEIVER,
	EXECUTOR
};

const char *bundleAgentProcName[NUM_PROCS] = {"information_exchange", "queueManager", "processor", "receiver", "executor"};

#define PROCS_PREFIX_PATH "/home/xeri/projects/adtn/root/bin"
#define CONF_PATH "/home/xeri/projects/adtn/test/adtn.ini"
/**/

enum event_e {
	TIMESTAMP
};

/* Monitorized process SHM 
 * Each monitorized process can access this struct from 
 * a shm region mapped into its memory space.
 */

typedef struct shmEventInfo_s {
	event_e event;		
	time_t timestamp;		
	int read;				
} shmEeventInfo_t;

typedef struct shmEventCtx_s {
	pthread_mutex_t event_m;
	pthread_cond_t event_c;
} shmEventCtx_t;
/**/


/* References from monitorizing process */


typedef struct eventInfo_s {
	BPatch_variableExpr *event;			// event_e (int)
	BPatch_variableExpr *timestamp;		// time_t (long)
	BPatch_variableExpr *read;			// int
} eventInfo_t;

typedef struct eventCtx_s {
	int init;
	BPatch_variableExpr *event_m;			// pthread_mutex_t
	BPatch_variableExpr *event_c;			// pthread_cond_t
	eventInfo_s eventInfo;
} eventCtx_t;
/**/

typedef struct bundleAgentProcCtx_s {
	const char *name;
	BPatch_addressSpace *handle;
	eventCtx_t eventCtx;
} bundleAgentProcCtx_t;

typedef struct testEnv_s {
	bundleAgentProcCtx_t procs[NUM_PROCS];
} testEnv_t;

using namespace std;

// Global BPatch instance. Represents the entire Dyninst library.
BPatch bpatch;

// Global test environment;
testEnv_t testEnv;

int runPlatform(const char *procsPrefix ,const char *confFile)
{
	int i = 0, ret = 1;
	char procCmd[PATH_MAX];
	const char *argv[5];

	for (i = 0; i< NUM_PROCS; i++){
		snprintf(procCmd, PATH_MAX, "%s/%s", procsPrefix, bundleAgentProcName[i]);
		argv[0] = procCmd;
		argv[1] = "-c";
		argv[2] = confFile;
		if (i == 0){
			argv[3] = "-f";
			argv[4] = NULL;
		} else {
			argv[3] = NULL;
		}

		testEnv.procs[i].name = bundleAgentProcName[i];
		testEnv.procs[i].handle = bpatch.processCreate(procCmd, argv);
		if (testEnv.procs[i].handle == NULL){
			fprintf(stderr, "Error starting %s", bundleAgentProcName[i]);
			goto end;
		}
		sleep(1);
	}

	ret = 0;
end:
	return ret;
}

int prepareProcMemory(bundleAgentProc_e proc)
{
	if (!testEnv.procs[proc].eventCtx.init){
		// Init event syncronization
		testEnv.procs[proc].eventCtx.event_m =  testEnv.procs[proc].handle->malloc(sizeof(pthread_mutex_t));
		testEnv.procs[proc].eventCtx.event_c =  testEnv.procs[proc].handle->malloc(sizeof(pthread_cond_t));

		// Init event info
		testEnv.procs[proc].eventCtx.eventInfo.event = testEnv.procs[proc].handle->malloc(sizeof(int));
		testEnv.procs[proc].eventCtx.eventInfo.timestamp = testEnv.procs[proc].handle->malloc(sizeof(time_t));
		testEnv.procs[proc].eventCtx.eventInfo.read = testEnv.procs[proc].handle->malloc(sizeof(int));
		int readInit = 1;
		testEnv.procs[proc].eventCtx.eventInfo.read->writeValue(&readInit);

		testEnv.procs[proc].eventCtx.init = 1;

		return 0;
	} else {
		fprintf(stderr, "Process %s already initialized", bundleAgentProcName[proc]);

		return 1;
	}
}

vector<BPatch_point *> *getEntryPoints(bundleAgentProc_e proc, const char *funcName)
{
	vector<BPatch_function *> functions;
	vector<BPatch_point *> *points = NULL;

	// Find function entry points
	BPatch_image *appImage = testEnv.procs[proc].handle->getImage();
	appImage->findFunction(funcName, functions);
	if (functions.size() == 0){
		fprintf(stderr, "Can't find function %s\n", funcName);
		goto end;
	} else if (functions.size() > 1) {
		printf("Warning more than one function %s found\n", funcName);
	}

	points = functions[0]->findPoint(BPatch_entry);
	if (points->size() == 0){
		fprintf(stderr, "Can't find function %s entry points\n", funcName);
		goto end;		
	} else if (points->size() > 1){
		printf("Warning more than one entry point in function %s found\n", funcName);
	}
	
end:
	return points;
};



int eventStartFunc(bundleAgentProc_e proc, /*out*/vector<BPatch_snippet *> &funcs)
{
	/*
	 * eventCtx.pthread_mutex_lock(&event_m) 
	 */
	vector<BPatch_snippet *> args;
	vector<BPatch_function *> targetFuncs;

	BPatch_image *appImage = testEnv.procs[proc].handle->getImage();
	eventCtx_t *eventCtx = &testEnv.procs[proc].eventCtx;

	// Construct args
	BPatch_constExpr mutexAddr(eventCtx->event_m->getBaseAddr());
	args.push_back(&mutexAddr);
	
	// Construct function
	appImage->findFunction("pthread_mutex_lock", targetFuncs);
	BPatch_funcCallExpr *lockMutex = new BPatch_funcCallExpr(*(targetFuncs[0]), args);
	funcs.push_back(lockMutex);

	// Clean
	args.clear();
	targetFuncs.clear();

	return 0;
}

int eventEndFunc(bundleAgentProc_e proc, event_e event, /*out*/vector<BPatch_snippet *> &funcs)
{
	/*
	 *	eventInfo.event = event;
	 *	eventCtx.pthread_cond_signal(&event_c)
	 *	eventCtx.pthread_mutex_unlock(&event_m)
	 */
	vector<BPatch_snippet *> args;
	vector<BPatch_function *> targetFuncs;

	BPatch_image *appImage = testEnv.procs[proc].handle->getImage();
	eventCtx_t *eventCtx = &testEnv.procs[proc].eventCtx;

	/* eventInfo.event = event */
	BPatch_arithExpr *assignEvent = new BPatch_arithExpr(BPatch_assign, *eventCtx->eventInfo.event, BPatch_constExpr(event));
	funcs.push_back(assignEvent);

	/* eventCtx.pthread_cond_signal(event_c) */
	// Construct args
	BPatch_constExpr condAddr(eventCtx->event_c->getBaseAddr());
	args.push_back(&condAddr);

	// Construct func
	appImage->findFunction("pthread_cond_signal", targetFuncs);
	BPatch_funcCallExpr *signalCond = new BPatch_funcCallExpr(*(targetFuncs[0]), args);
	funcs.push_back(signalCond);

	// Clean
	args.clear();
	targetFuncs.clear();

	/* eventCtx.pthread_mutex_unlock(event_m) */
	// Construct args
	BPatch_constExpr mutexAddr(eventCtx->event_m->getBaseAddr());
	args.push_back(&mutexAddr);
	
	// Construct function
	appImage->findFunction("pthread_mutex_unlock", targetFuncs);
	BPatch_funcCallExpr *lockMutex =  new BPatch_funcCallExpr(*(targetFuncs[0]), args);
	funcs.push_back(lockMutex);

	// Clean
	args.clear();
	targetFuncs.clear();

	return 0;
}


int timestampFunc(bundleAgentProc_e proc, vector<BPatch_snippet *> &funcs)
{
	/*
	 *	clock_gettime(CLOCK_MONOTONIC_RAW, &eventInfo.timestamp);
	 */	

	vector<BPatch_snippet *> args;
	vector<BPatch_function *> targetFuncs;

	BPatch_image *appImage = testEnv.procs[proc].handle->getImage();
	eventCtx_t *eventCtx = &testEnv.procs[proc].eventCtx;

	// Construct args
	BPatch_constExpr clock_gettimeClkId(CLOCK_MONOTONIC_RAW);
	args.push_back(&clock_gettimeClkId);
	BPatch_constExpr timestampAddr(eventCtx->eventInfo.timestamp->getBaseAddr());
	args.push_back(&timestampAddr);

	// Construct function
	appImage->findFunction("clock_gettime", targetFuncs);
	BPatch_funcCallExpr *lockMutex = new BPatch_funcCallExpr(*(targetFuncs[0]), args);
	funcs.push_back(lockMutex);	

	// Clean
	args.clear();
	targetFuncs.clear();

	return 0;
}

int main(int argc, char const *argv[])
{

	/* Initialization */
	bzero(&testEnv, sizeof(testEnv));
	setenv("DYNINSTAPI_RT_LIB", DYNINSTAPI_RT_LIB, 1);
	/**/

	/* Optimizations */
	// Turn on or off trampoline recursion. By default, any snippets invoked while another snippet is active will not be executed. 
	bpatch.setTrampRecursive(true);
	// Turn on or off floating point saves. 
	bpatch.setSaveFPR(false);
	/**/

	/* Start platform */
	if (runPlatform(PROCS_PREFIX_PATH, CONF_PATH) != 0){
		fprintf(stderr, "Error starting platform.\n");
		exit(1);
	}
	/**/

	/* Prepare proc memory */
	prepareProcMemory(RECEIVER);
	/**/


	//TEMP
	vector<BPatch_point *> *points = getEntryPoints(RECEIVER, "init_adtn_process");
	BPatch_addressSpace *app = testEnv.procs[RECEIVER].handle;

	// Insert eventStartSnippet
	vector<BPatch_snippet *> eventStartSnippet = vector<BPatch_snippet *>();
	eventStartFunc(RECEIVER, eventStartSnippet);
	app->insertSnippet(*(eventStartSnippet[0]), *points);

	// Insert snipept
	vector<BPatch_snippet *> timestampSnippet = vector<BPatch_snippet *>();
	timestampFunc(RECEIVER, timestampSnippet);
	app->insertSnippet(*(timestampSnippet[0]), *points);

	BPatch_process *appProc = dynamic_cast<BPatch_process *>(app);
	appProc->continueExecution();

	// // Insert eventEndSnippet
	// vector<BPatch_snippet *> eventEndSnippet = vector<BPatch_snippet *>();
	// eventEndFunc(RECEIVER, TIMESTAMP, eventEndSnippet);
	// app->insertSnippet(*(eventEndSnippet[0]), *points);

	// pthread_mutex_lock((pthread_mutex_t *)testEnv.procs[RECEIVER].eventCtx.event_m->getBaseAddr());
	// while (*(int *)testEnv.procs[RECEIVER].eventCtx.eventInfo.read->getBaseAddr() == 1){
	// 	pthread_cond_wait((pthread_cond_t *)testEnv.procs[RECEIVER].eventCtx.event_c->getBaseAddr(),
	// 		(pthread_mutex_t *)testEnv.procs[RECEIVER].eventCtx.event_m->getBaseAddr());
	// }

	sleep(5);

	// Check timestmap
	struct timespec ts;
	testEnv.procs[RECEIVER].eventCtx.eventInfo.timestamp->readValue(&ts);
	printf("ts.tv_sec: %ld\n", ts.tv_sec);

	pthread_mutex_unlock((pthread_mutex_t *)testEnv.procs[RECEIVER].eventCtx.event_m->getBaseAddr());

	/* Hook handlers */
	/**/

	/* Start processes */
	/**/

	/* Analize resutls */
	/**/


	return 0;
}



// Get function entry points
//vector<BPatch_point *> *entryPoints = getEntryPoints(proc, "pthread_mutex_lock");




// int setHook(bundleAgentProc_e proc, hook_e hook)
// {

// 	if (hook == TIMESTAMP){

// 	}

// }

