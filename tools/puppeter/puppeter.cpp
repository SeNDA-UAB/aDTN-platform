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
#include <sys/mman.h>
#include <fcntl.h>

//CPP
#include <string>

#define DYNINSTAPI_RT_LIB "/usr/local/lib64/libdyninstAPI_RT.so"
#define PROCS_PREFIX_PATH "/home/xeri/projects/adtn/root/bin"
#define CONF_PATH "/home/xeri/projects/adtn/test/adtn.ini"

/* Puppet procs */
#define NUM_PROCS 5
enum puppetProc_e {
	INFORMATION_EXCHANGE,
	QUEUEMANAGER,
	PROCESSOR,
	RECEIVER,
	EXECUTOR
};
const char *puppetProcName[NUM_PROCS] = {"information_exchange", "queueManager", "processor", "receiver", "executor"};
/**/

/* Events */
#define NUM_EVENTS 1
enum event_e {
	TIMESTAMP
};

typedef struct eventHandler_s{
	event_e event;
	int (*f)(puppetProc_e, vector<BPatch_snippet *>, void*);
} eventHandler_t;

eventHandler_t eventHandlers[NUM_EVENTS];
/**/

/* Event CTX */

/* Monitorized process SHM 
 * Each monitorized process can access this struct from 
 * a shm region mapped into its memory space.
 */
typedef struct shmEventInfo_s {
	event_e event;		
	struct timespec timestamp;		
	int read;				
} shmEeventInfo_t;

typedef struct shmEventCtx_s {
	pthread_mutex_t event_m;
	pthread_cond_t event_c;
	shmEeventInfo_t info;
} shmEventCtx_t;

/* SHM addresses from the mutator and the mutatee */
typedef struct eventCtx_s {
	int init;

	// mutator
	int shmFd;
	shmEventCtx_t *shm;

	// mutatee
	int shmFd_m;
	shmEventCtx_t *shm_m;
} eventCtx_t;
/**/

/* Dyninst CTX */
typedef struct puppetProcCtx_s {
	const char *name;
	BPatch_addressSpace *handle;
	eventCtx_t eventCtx;
} puppetProcCtx_t;

typedef struct testEnv_s {
	puppetProcCtx_t procs[NUM_PROCS];
} testEnv_t;
/**/

using namespace std;

// Global BPatch instance. Represents the entire Dyninst library.
BPatch bpatch;

// Global test environment;
testEnv_t testEnv;

/* Dyninst util functions */
vector<BPatch_point *> *getEntryPoints(puppetProc_e proc, const char *funcName)
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
/**/

/* Creates a shared memory region to be shared between the monitorized and the monitorizing process.
 * Initializes structs in shm.
 * Maps this region into the two processes memory spaces.
 */
int prepareProcMemory(puppetProc_e proc)
{
	char shmName[32];
	if (!testEnv.procs[proc].eventCtx.init){
		BPatch_addressSpace *app = testEnv.procs[proc].handle;
		BPatch_image *appImage = app->getImage();
		BPatch_process *appProc = dynamic_cast<BPatch_process *>(app);

		/* Create and map shared memory region into mutator process. */

		snprintf(shmName, sizeof(shmName), "/%d-%d", getpid(), appProc->getPid());
		testEnv.procs[proc].eventCtx.shmFd = shm_open(shmName, O_RDWR | O_CREAT | O_EXCL, S_IRWXU | S_IRWXG);
		ftruncate(testEnv.procs[proc].eventCtx.shmFd, sizeof(shmEventCtx_t));
		testEnv.procs[proc].eventCtx.shm = (shmEventCtx_t *) mmap(NULL, sizeof(shmEventCtx_t), PROT_READ | PROT_WRITE, MAP_SHARED, testEnv.procs[proc].eventCtx.shmFd, 0);
		bzero(testEnv.procs[proc].eventCtx.shm, sizeof(shmEventCtx_t));

		// Init mutex
		pthread_mutexattr_t event_mAttr;
		pthread_mutexattr_init(&event_mAttr);
		pthread_mutexattr_setpshared(&event_mAttr, PTHREAD_PROCESS_SHARED);
		pthread_mutex_init(&testEnv.procs[proc].eventCtx.shm->event_m, &event_mAttr);

		// Init cond
		pthread_condattr_t event_cAttr;
		pthread_condattr_init(&event_cAttr);
    	pthread_condattr_setpshared(&event_cAttr, PTHREAD_PROCESS_SHARED);
    	pthread_cond_init(&testEnv.procs[proc].eventCtx.shm->event_c, &event_cAttr);


    	/* Map shared memory region into mutatee process */

    	// Creating and executing function:
    	// int shmFd_m = shm_open(shmName, O_RDWR, S_IRWXU | S_IRWXG);

		// Find shm_open functions
		vector<BPatch_function *>shm_openFuncs; 
		appImage->findFunction("shm_open", shm_openFuncs); 

		// Prepare args
		vector<BPatch_snippet *> shm_openArgs;
		BPatch_constExpr shm_open_name(shmName);
		BPatch_constExpr shm_open_oflag(O_RDWR);
 		BPatch_constExpr shm_open_mode(S_IRWXU | S_IRWXG); 

 		shm_openArgs.push_back(&shm_open_name);
 		shm_openArgs.push_back(&shm_open_oflag);
 		shm_openArgs.push_back(&shm_open_mode);

 		// Create procedure call
 		BPatch_funcCallExpr shm_openCall(*shm_openFuncs[0], shm_openArgs); 

 		// Execute shm_open and get returned fd 
 		testEnv.procs[proc].eventCtx.shmFd_m = (uintptr_t) appProc->oneTimeCode(shm_openCall); 


		// Creating and executing function:
		// void *shm_m = mmap(NULL, sizeof(shmEventCtx_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		
		// Fund mmap functions
		vector<BPatch_function *>mmapFuncs; 
		appImage->findFunction("mmap", mmapFuncs); 

		// Prepare args
		vector<BPatch_snippet *> mmapArgs;
		BPatch_constExpr mmap_addr(0);
		BPatch_constExpr mmap_length(sizeof(shmEventCtx_t));
 		BPatch_constExpr mmap_prot(PROT_READ | PROT_WRITE); 
 		BPatch_constExpr mmap_flags(MAP_SHARED); 
 		BPatch_constExpr mmap_fd(testEnv.procs[proc].eventCtx.shmFd_m); 
 		BPatch_constExpr mmap_offset(0); 

 		mmapArgs.push_back(&mmap_addr);
 		mmapArgs.push_back(&mmap_length);
 		mmapArgs.push_back(&mmap_prot);
 		mmapArgs.push_back(&mmap_flags);
 		mmapArgs.push_back(&mmap_fd);
 		mmapArgs.push_back(&mmap_offset);

 		// Create procedure call
 		BPatch_funcCallExpr mmapCall(*mmapFuncs[0], mmapArgs); 

 		// Execute mmap and get returned addr.
 		testEnv.procs[proc].eventCtx.shm_m = (shmEventCtx_t *)appProc->oneTimeCode(mmapCall); 

 		// Init state
 		testEnv.procs[proc].eventCtx.shm->info.read = 1;
		testEnv.procs[proc].eventCtx.init = 1;

		return 0;
	} else {
		fprintf(stderr, "Process %s already initialized", puppetProcName[proc]);

		return 1;
	}
}

/* Events */
int eventStartFunc(puppetProc_e proc, /*out*/vector<BPatch_snippet *> &funcs)
{
	/*
	 * eventCtx.pthread_mutex_lock(&event_m) 
	 */
	vector<BPatch_snippet *> args;
	vector<BPatch_function *> targetFuncs;

	BPatch_image *appImage = testEnv.procs[proc].handle->getImage();
	shmEventCtx_t *shm_mAddr = (shmEventCtx_t *)testEnv.procs[proc].eventCtx.shm_m;

	// Construct args
	BPatch_constExpr mutexAddr(&shm_mAddr->event_m);
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

int eventEndFunc(puppetProc_e proc, event_e event, /*out*/vector<BPatch_snippet *> &funcs)
{
	/*
	 *	eventInfo.event = event;
	 *	eventInfo.read = 0;
	 *	eventCtx.pthread_cond_signal(&event_c)
	 *	eventCtx.pthread_mutex_unlock(&event_m)
	 */

	BPatch_image *appImage = testEnv.procs[proc].handle->getImage();
	shmEventCtx_t *shm_mAddr = (shmEventCtx_t *)testEnv.procs[proc].eventCtx.shm_m;

	/* eventInfo.event = event */
	BPatch_variableExpr *eventType = testEnv.procs[proc].handle->createVariable(Dyninst::Address(&shm_mAddr->info.event), bpatch.createScalar("event", sizeof(event_e)));
	BPatch_arithExpr *assignEvent = new BPatch_arithExpr(BPatch_assign, *eventType, BPatch_constExpr(event));
	funcs.push_back(assignEvent);
	free(eventType);

	/* eventInfo.read = 0 */
	BPatch_variableExpr *read = testEnv.procs[proc].handle->createVariable(Dyninst::Address(&shm_mAddr->info.read), bpatch.createScalar("read", sizeof(int)));
	BPatch_arithExpr *assignRead = new BPatch_arithExpr(BPatch_assign, *read, BPatch_constExpr(0));
	funcs.push_back(assignRead);	
	free(read);

	/* eventCtx.pthread_cond_signal(event_c) */

	// Construct args
	vector<BPatch_snippet *> condArgs;
	BPatch_constExpr condAddr(&shm_mAddr->event_c);
	condArgs.push_back(&condAddr);
	
	// Construct func
	vector<BPatch_function *> condFuncs;
	appImage->findFunction("pthread_cond_signal", condFuncs);
	BPatch_funcCallExpr *signalCond = new BPatch_funcCallExpr(*(condFuncs[1]), condArgs);
	funcs.push_back(signalCond);
	
	/* eventCtx.pthread_mutex_unlock(event_m) */
	
	// Construct args
	vector<BPatch_snippet *> mutexArgs;
	BPatch_constExpr mutexAddr(&shm_mAddr->event_m);
	mutexArgs.push_back(&mutexAddr);
	
	// Construct function
	vector<BPatch_function *> mutexFuncs;
	appImage->findFunction("pthread_mutex_unlock", mutexFuncs);
	BPatch_funcCallExpr *unlockMutex =  new BPatch_funcCallExpr(*(mutexFuncs[0]), mutexArgs);
	funcs.push_back(unlockMutex);
	
	
	return 0;
}


int timestampFunc(puppetProc_e proc, vector<BPatch_snippet *> &funcs, void *data)
{
	/*
	 *	clock_gettime(CLOCK_MONOTONIC_RAW, &eventInfo.timestamp);
	 */	

	vector<BPatch_snippet *> args;
	vector<BPatch_function *> targetFuncs;

	BPatch_image *appImage = testEnv.procs[proc].handle->getImage();
	shmEventCtx_t *shm_mAddr = (shmEventCtx_t *)testEnv.procs[proc].eventCtx.shm_m;

	// Construct args
	BPatch_constExpr clock_gettimeClkId(CLOCK_MONOTONIC_RAW);
	args.push_back(&clock_gettimeClkId);
	BPatch_constExpr timestampAddr(&shm_mAddr->info.timestamp);
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
/**/

/* Commands */
int initEnvironment(){

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

	return 0;
}

int initPlatform(const char *procsPrefix ,const char *confFile)
{
	int i = 0, ret = 1;
	char procCmd[PATH_MAX];
	const char *argv[5];

	for (i = 0; i< NUM_PROCS; i++){
		snprintf(procCmd, PATH_MAX, "%s/%s", procsPrefix, puppetProcName[i]);
		argv[0] = procCmd;
		argv[1] = "-c";
		argv[2] = confFile;
		if (i == 0){
			argv[3] = "-f";
			argv[4] = NULL;
		} else {
			argv[3] = NULL;
		}

		testEnv.procs[i].name = puppetProcName[i];
		testEnv.procs[i].handle = bpatch.processCreate(procCmd, argv);
		if (testEnv.procs[i].handle == NULL){
			fprintf(stderr, "Error starting %s", puppetProcName[i]);
			goto end;
		}
		sleep(1);
	}

	ret = 0;
end:
	return ret;
}

int addEvent(puppetProc_e proc, event_e event, const char *function)
{
	// Prepare mutatee memory
	if (!testEnv.procs[proc].eventCtx.init)
		prepareProcMemory(RECEIVER);

	// Find function into mutatee process
	vector<BPatch_point *> *points = getEntryPoints(proc, function);
	if (points->size() == 0){
		fprintf(stderr, "Function %s not found in process %s", function, puppetProcName[proc]);
		return 1;
	} else if (points->size() > 1){
		fprintf(stderr, "Warning more than one function %s found in process %s", function, puppetProcName[proc]);
	}

	// Start snippets insertion 
	BPatch_addressSpace *app = testEnv.procs[proc].handle;
	app->beginInsertionSet();

	// Insert eventStartSnippet
	vector<BPatch_snippet *> eventStartSnippet = vector<BPatch_snippet *>();
	eventStartFunc(proc, eventStartSnippet);
	app->insertSnippet(*(eventStartSnippet[0]), *points, BPatch_callBefore, BPatch_lastSnippet);

	// Insert snipept
	vector<BPatch_snippet *> eventSnippet = vector<BPatch_snippet *>();

	timestampFunc(proc, eventSnippet);
	app->insertSnippet(*(eventSnippet[0]), *points, BPatch_callBefore, BPatch_lastSnippet);

	// Insert eventEnd snippet
	vector<BPatch_snippet *> eventEndSnippet = vector<BPatch_snippet *>();
	eventEndFunc(proc, TIMESTAMP, eventEndSnippet);
	for(std::vector<BPatch_snippet *>::iterator it = eventEndSnippet.begin(); it != eventEndSnippet.end(); ++it) {
		app->insertSnippet(**it, *points, BPatch_callBefore, BPatch_lastSnippet);
	}

	app->finalizeInsertionSet(true);

	return 0;
}

/**/




void *read_timestamp(void *)
{
	pthread_mutex_lock((pthread_mutex_t *)&testEnv.procs[RECEIVER].eventCtx.shm->event_m);

	while (testEnv.procs[RECEIVER].eventCtx.shm->info.read == 1)
		pthread_cond_wait((pthread_cond_t *)&testEnv.procs[RECEIVER].eventCtx.shm->event_c, (pthread_mutex_t *)&testEnv.procs[RECEIVER].eventCtx.shm->event_m);

	// Check timestmap
	printf("timestamp.tv_sec: %ld\n", testEnv.procs[RECEIVER].eventCtx.shm->info.timestamp.tv_sec);
	fflush(stdout);

	pthread_mutex_unlock((pthread_mutex_t *)&testEnv.procs[RECEIVER].eventCtx.shm->event_m);

	return NULL;
}

int main(int argc, char const *argv[])
{

	if (initEnvironment() != 0){
		fprintf(stderr, "Error initializing test environment.\n");
		exit(1);		
	}

	if (initPlatform(PROCS_PREFIX_PATH, CONF_PATH) != 0){
		fprintf(stderr, "Error starting platform.\n");
		exit(1);
	}






	/* Start monitoring threads */
	pthread_t read_timestamp_t;
	pthread_create(&read_timestamp_t,  NULL, read_timestamp, NULL);

	/**/

	
	BPatch_process *appProc = dynamic_cast<BPatch_process *>(app);
	appProc->continueExecution();

	while (!appProc->isTerminated()) {
		bpatch.waitForStatusChange();
	}

	/* Hook handlers */
	/**/

	/* Start processes */
	/**/

	/* Analize resutls */
	/**/

	fflush(stdout);

	return 0;
}