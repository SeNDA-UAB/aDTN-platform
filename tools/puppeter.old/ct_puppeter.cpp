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
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <sys/un.h>

// aDTN
extern "C" {
#include "bundle.h"
#include "utils.h"
#include "executor.h"
}

//CPP
#include <string>
#include <cstring>
//Utils
#include "utlist.h"

#define DYNINSTAPI_RT_LIB "/usr/local/lib64/libdyninstAPI_RT.so"
#define PROCS_PREFIX_PATH "/home/xeri/projects/adtn/root/bin"
#define CONF_PATH "/home/xeri/projects/adtn/test/adtn.ini"

/* Puppet procs */
#define NUM_PROCS 1
enum puppetProc_e {
	EXECUTOR
};
const char *puppetProcName[NUM_PROCS] = {"executor"};
/**/

/* Events */
#define MAX_DATA 512    // Bytes
#define NUM_EVENTS 1    // Increase if more events are added
enum event_e {
	TIMESTAMP
};

enum timestamp_id_e {
	EXEC_START_COMPILATION = 1,
	EXEC_END_COMPILATION
};

int timestampSnippet(puppetProc_e proc, vector<BPatch_snippet *> &funcs, void *data);

typedef struct eventHandler_s {
	event_e event;
	int (*f)(puppetProc_e, vector<BPatch_snippet *> &, void *);
} eventHandler_t;

typedef struct event_s {
	event_e event;
	struct timespec timestamp;
	uint8_t data[MAX_DATA];
	struct event_s *next;
} event_t;
/**/

/* Event CTX */
// Monitorized process SHM
// Each monitorized process can access this struct from
// a shm region mapped into its memory space.
typedef struct shmEventInfo_s {
	event_e event;
	struct timespec timestamp;
	uint8_t data[MAX_DATA];
	int read;
} shmEeventInfo_t;

typedef struct shmEventCtx_s {
	pthread_mutex_t event_m;
	pthread_cond_t event_c;
	shmEeventInfo_t info;
} shmEventCtx_t;

// SHM addresses from the mutator and the mutatee
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
	eventHandler_t eventHandlers[NUM_EVENTS];
	event_t *eventQueue;
} testEnv_t;
/**/

using namespace std;

// Global BPatch instance. Represents the entire Dyninst library.
BPatch bpatch;

// Global test environment;
testEnv_t testEnv;

/********** Dyninst util functions **********/

// loc= enum BPatch_procedureLocation {
// BPatch_entry,
// BPatch_exit,
// BPatch_subroutine,
// BPatch_locInstruction,
// BPatch_locBasicBlockEntry,
// BPatch_locLoopEntry,
// BPatch_locLoopExit,
// BPatch_locLoopStartIter,
// BPatch_locLoopStartExit,
// BPatch_allLocations }
//
vector<BPatch_point *> *getEntryPoints(puppetProc_e proc, const char *funcName, BPatch_procedureLocation loc)
{
	vector<BPatch_function *> functions;
	vector<BPatch_point *> *points = NULL;

	// Find function entry points
	BPatch_image *appImage = testEnv.procs[proc].handle->getImage();
	appImage->findFunction(funcName, functions);
	if (functions.size() == 0) {
		fprintf(stderr, "Can't find function %s\n", funcName);
		goto end;
	} else if (functions.size() > 1) {
		printf("Warning more than one function %s found\n", funcName);
	}

	points = functions[0]->findPoint(loc);
	if (points->size() == 0) {
		fprintf(stderr, "Can't find function %s entry points\n", funcName);
		goto end;
	} else if (points->size() > 1) {
		printf("Warning more than one entry point in function %s found\n", funcName);
	}

end:
	return points;
};

/********************/

/*********** Events managment **********/

/* Creates a shared memory region to be shared between the monitorized and the monitorizing process.
 * Initializes structs in shm.
 * Maps this region into the two processes memory spaces.
 */
int prepareProcMemory(puppetProc_e proc)
{
	char shmName[32];
	if (!testEnv.procs[proc].eventCtx.init) {
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

int registerEvents()
{
	testEnv.eventHandlers[TIMESTAMP].f = timestampSnippet;

	return 0;
}

int enqueueEvent(event_t *ev)
{
	LL_APPEND(testEnv.eventQueue, ev);

	return 0;
}

/*********************/

/*********** Event Snippets **********/

int eventStartSnippet(puppetProc_e proc, /*out*/vector<BPatch_snippet *> &funcs)
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

int eventEndSnippet(puppetProc_e proc, event_e event, /*out*/vector<BPatch_snippet *> &funcs)
{
	/*
	 *  eventInfo.event = event;
	 *  eventInfo.read = 0;
	 *  eventCtx.pthread_cond_signal(&event_c)
	 *  eventCtx.pthread_mutex_unlock(&event_m)
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

int timestampSnippet(puppetProc_e proc, vector<BPatch_snippet *> &funcs, void *data)
{
	vector<BPatch_snippet *> args;
	vector<BPatch_function *> targetFuncs;

	BPatch_image *appImage = testEnv.procs[proc].handle->getImage();
	shmEventCtx_t *shm_mAddr = (shmEventCtx_t *)testEnv.procs[proc].eventCtx.shm_m;


	/* *eventInfo.data = *data; */
	BPatch_variableExpr *BP_data = testEnv.procs[proc].handle->createVariable(Dyninst::Address(&shm_mAddr->info.data), bpatch.createScalar("data", sizeof(event_e)));
	BPatch_arithExpr *cpData = new BPatch_arithExpr(BPatch_assign, *BP_data, BPatch_constExpr(*(timestamp_id_e *)data)); // Just copy the first byte
	funcs.push_back(cpData);
	free(BP_data);

	/* clock_gettime(CLOCK_REALTIME, &eventInfo.timestamp); */

	// Construct args
	BPatch_constExpr clock_gettimeClkId(CLOCK_REALTIME);
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

/********************/


/********** Event listener **********/

void *eventListenerThread(void *proc_p)
{
	puppetProc_e proc = *(puppetProc_e *)proc_p;
	do {
		pthread_mutex_lock((pthread_mutex_t *)&testEnv.procs[proc].eventCtx.shm->event_m);

		// Wait until there is a new event
		while (testEnv.procs[proc].eventCtx.shm->info.read == 1)
			pthread_cond_wait((pthread_cond_t *)&testEnv.procs[proc].eventCtx.shm->event_c, (pthread_mutex_t *)&testEnv.procs[proc].eventCtx.shm->event_m);


		// Store event info
		event_t *ev = (event_t *)calloc(1, sizeof(event_t));
		ev->event = testEnv.procs[proc].eventCtx.shm->info.event;
		ev->timestamp = testEnv.procs[proc].eventCtx.shm->info.timestamp;
		ev->data[0] = testEnv.procs[proc].eventCtx.shm->info.data[0];
		enqueueEvent(ev);

		// Set event as read
		testEnv.procs[proc].eventCtx.shm->info.read = 1;

		pthread_mutex_unlock((pthread_mutex_t *)&testEnv.procs[proc].eventCtx.shm->event_m);

	} while (1);

	return NULL;
}

/********************/


/********** Puppeter commands **********/

int initEnvironment()
{
	/* Initialization */
	bzero(&testEnv, sizeof(testEnv));
	setenv("DYNINSTAPI_RT_LIB", DYNINSTAPI_RT_LIB, 1);
	registerEvents();
	/**/

	/* Optimizations */
	// Turn on or off trampoline recursion. By default, any snippets invoked while another snippet is active will not be executed.
	bpatch.setTrampRecursive(true);
	// Turn on or off floating point saves.
	bpatch.setSaveFPR(false);
	/**/

	return 0;
}

int initProcs(const char *procsPrefix , const char *confFile)
{
	int i = 0, ret = 1;
	char procCmd[PATH_MAX];
	const char *argv[5];

	for (i = 0; i < NUM_PROCS; i++) {
		snprintf(procCmd, PATH_MAX, "%s/%s", procsPrefix, puppetProcName[i]);
		argv[0] = procCmd;
		argv[1] = "-c";
		argv[2] = confFile;
		if (i == 0) {
			argv[3] = "-f";
			argv[4] = NULL;
		} else {
			argv[3] = NULL;
		}

		testEnv.procs[i].name = puppetProcName[i];
		testEnv.procs[i].handle = bpatch.processCreate(procCmd, argv);
		if (testEnv.procs[i].handle == NULL) {
			fprintf(stderr, "Error starting %s", puppetProcName[i]);
			goto end;
		}
	}

	ret = 0;
end:
	return ret;
}

// when=[BPatch_callBefore| BPatch_callAfter]
int hookEvent(puppetProc_e proc, event_e event, BPatch_callWhen when, void *data, const char *function)
{
	vector<BPatch_point *> *points;
	// Prepare mutatee memory
	if (!testEnv.procs[proc].eventCtx.init)
		prepareProcMemory(proc);

	// Find function into mutatee process
	if (when == BPatch_callBefore) {
		points = getEntryPoints(proc, function, BPatch_entry);
	} else {
		points = getEntryPoints(proc, function, BPatch_exit);
	}
	if (!points) {
		fprintf(stderr, "Function %s not found in process %s\n", function, puppetProcName[proc]);
		return 1;
	} else if (points->size() > 1) {
		fprintf(stderr, "Warning more than one function %s found in process %s\n", function, puppetProcName[proc]);
	}

	// Start snippets insertion
	BPatch_addressSpace *app = testEnv.procs[proc].handle;
	app->beginInsertionSet();

	// Insert eventStartSnippet
	vector<BPatch_snippet *> startSnippet = vector<BPatch_snippet *>();
	eventStartSnippet(proc, startSnippet);
	for (vector<BPatch_snippet *>::iterator it = startSnippet.begin(); it != startSnippet.end(); ++it) {
		app->insertSnippet(**it, *points, when, BPatch_lastSnippet);
	}

	// Insert event snipept
	vector<BPatch_snippet *> eventSnippet = vector<BPatch_snippet *>();
	testEnv.eventHandlers[event].f(proc, eventSnippet, data);
	for (vector<BPatch_snippet *>::iterator it = eventSnippet.begin(); it != eventSnippet.end(); ++it) {
		app->insertSnippet(**it, *points, when, BPatch_lastSnippet);
	}

	// Insert eventEnd snippet
	vector<BPatch_snippet *> endSnippet = vector<BPatch_snippet *>();
	eventEndSnippet(proc, TIMESTAMP, endSnippet);
	for (vector<BPatch_snippet *>::iterator it = endSnippet.begin(); it != endSnippet.end(); ++it) {
		app->insertSnippet(**it, *points, when, BPatch_lastSnippet);
	}

	app->finalizeInsertionSet(true);

	return 0;
}

pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

void *endProcsThread(void *timeout_secs)
{
	int i, sig;
	siginfo_t si;
	sigset_t blockedSigs = {{0}};
	struct timespec timeout = {0};
	timeout.tv_sec = *(int *)timeout_secs;

	sigaddset(&blockedSigs, SIGINT);
	sigaddset(&blockedSigs, SIGTERM);

	do {
		sig = sigtimedwait(&blockedSigs, &si, &timeout);
	} while (sig < 0 && errno == EINTR);

	//pthread_mutex_lock(&m);
	for (i = NUM_PROCS - 1; i >= 0; i--) {
		BPatch_process *appProc = dynamic_cast<BPatch_process *>(testEnv.procs[i].handle);
		//appProc->terminateExecution();
		kill(appProc->getPid(), SIGINT);
	}
	//pthread_mutex_unlock(&m);

	free(timeout_secs);

	return NULL;
}


int startTest(const int timeout_secs)
{
	BPatch_process *appProc[NUM_PROCS];
	int ret = 0, i;

	// Block signals so new threads don't catch
	// sigset_t blockedSigs = {{0}};
	// sigaddset(&blockedSigs, SIGINT);
	// sigaddset(&blockedSigs, SIGTERM);
	// sigprocmask(SIG_BLOCK, &blockedSigs, NULL);

	printf("---> Test started\n");

	// Start event listener threads and mutatees.
	pthread_t threads[NUM_PROCS] = {0};
	for (i = 0; i < NUM_PROCS; i++) {
		if (testEnv.procs[i].eventCtx.init) {
			puppetProc_e *proc = (puppetProc_e *)malloc(sizeof(puppetProc_e));
			*proc = (puppetProc_e)i;
			if (pthread_create(&threads[i], NULL, eventListenerThread, (void *)proc) != 0) {
				fprintf(stderr, "Error initializing event listener thread for process %s", puppetProcName[i]);
				ret |= 1;
			}
		}
		appProc[i] = dynamic_cast<BPatch_process *>(testEnv.procs[i].handle);
		appProc[i]->continueExecution();
		usleep(500 * 10 ^ 3);
	}

	// Launch end thread
	pthread_t endThread;
	int *timeout_secs_p = (int *)malloc(sizeof(int));
	*timeout_secs_p = timeout_secs;
	if (pthread_create(&endThread, NULL, endProcsThread, (void *)timeout_secs_p)) {
		fprintf(stderr, "Error initializing end thread, test will never finsih.");
		ret |= 1;
	}

	return 0;
}

int waitTestEnd()
{
	int i;
	BPatch_process *appProc;

	// Wait until all processes have been terminated
	for (;;) {
		int all_terminated = 1;
		//pthread_mutex_lock(&m);
		for (i = 0; i < NUM_PROCS; i++) {
			appProc = dynamic_cast<BPatch_process *>(testEnv.procs[i].handle);
			if (!appProc->isTerminated()) {
				all_terminated = 0;
				break;
			}
		}
		//pthread_mutex_unlock(&m);

		if (all_terminated) {
			break;
		} else {
			bpatch.waitForStatusChange();
		}
	}

	// Cancels threads
	// for (i = NUM_PROCS - 1; i >= 0; i--) {
	//  if (threads[i] != 0) {
	//      pthread_cancel(threads[i]);
	//      pthread_join(threads[i], NULL);
	//  }
	// }

	printf("---> Test finished\n");

	return 0;
}

/********************/

/********** aDTN commands **********/

bundle_s *createBundle()
{
	bundle_s *b = bundle_new();

	// Primary block
	bundle_set_source(b, "local:1");
	bundle_set_destination(b, "xeri2:1");
	bundle_set_lifetime(b, 30);

	// Payload
	payload_block_s *p = bundle_new_payload_block();
	bundle_set_payload(p, (uint8_t *)"TEST_BUNDLE", strlen("TEST_BUNDLE"));

	// Put all together
	bundle_add_ext_block(b, (ext_block_s *)p);

	return b;
}

int sendBundle(const char *platformDataPath, bundle_s *b, /*out*/char **b_name)
{
	// Create bundle
	uint8_t *b_raw;
	int b_l = bundle_create_raw(b , &b_raw);

	// Write bundle to receiver incoming folder
	char input_path[PATH_MAX];
	snprintf(input_path, PATH_MAX, "%s/%s", platformDataPath, INPUT_PATH);

	*b_name = generate_bundle_name("local");
	write_to(input_path, *b_name, b_raw, b_l);

	return 0;
}

// TODO: Add it to queueManager
int storeBundleIntoQueue(const char *platformDataPath, bundle_s *b, /*out*/char **b_name)
{
	// Create bundle
	uint8_t *b_raw;
	int b_l = bundle_create_raw(b , &b_raw);

	// Write bundle to receiver incoming folder
	char input_path[PATH_MAX];
	snprintf(input_path, PATH_MAX, "%s/%s", platformDataPath, QUEUE_PATH);

	*b_name = generate_bundle_name("local");
	write_to(input_path, *b_name, b_raw, b_l);

	return 0;
}

int executeCode(const char *platformDataPath, const char *b_name)
{
	struct _petition p;
	union _response r;

	bzero(&p, sizeof(p));
	bzero(&r, sizeof(r));

	// Prepare executor comm.
	int test_s = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (test_s < 0) {
		perror("socket()");
		return 1;
	}
	struct sockaddr_un test_s_addr;
	bzero(&test_s_addr, sizeof(test_s_addr));
	test_s_addr.sun_family = AF_UNIX;
	srand(time(NULL));
	snprintf(test_s_addr.sun_path, sizeof(test_s_addr.sun_path), "/tmp/%d.sock", rand());
	unlink(test_s_addr.sun_path);
	if (bind(test_s, (struct sockaddr *)&test_s_addr, sizeof(struct sockaddr_un)) == -1) {
		perror("bind()");
		close(test_s);
		return 1;
	}

	// Prepare petition
	p.header.petition_type = EXE;
	p.header.code_type = ROUTING_CODE;
	strncpy(p.header.bundle_id, b_name, sizeof(p.header.bundle_id));

	// Send petition
	struct sockaddr_un exec_addr;
	bzero(&exec_addr, sizeof(exec_addr));
	exec_addr.sun_family = AF_UNIX;
	snprintf(exec_addr.sun_path, sizeof(exec_addr.sun_path), "/%s/%s", platformDataPath, "executor.sock");
	if (sendto(test_s, &p, sizeof(p), 0, (struct sockaddr *)&exec_addr, (socklen_t)sizeof(exec_addr)) < 0) {
		perror("sendto()");
		close(test_s);
		return 1;
	}

	// Get response
	int recv_l = recv(test_s, &r, sizeof(r), 0);
	if (recv_l < 0) {
		perror("recv()");
		close(test_s);
	}

	// Disconnect from executor
	close(test_s);

	return 0;
}

/********************/

double diff_time(struct timespec *start, struct timespec *end)
{
	return (double)(end->tv_sec - start->tv_sec) * 1.0e9 + (double)(end->tv_nsec - start->tv_nsec);
}

int main(int argc, char const *argv[])
{
	timestamp_id_e timestamp_id;;

	if (initEnvironment() != 0) {
		fprintf(stderr, "Error initializing test environment.\n");
		exit(1);
	}

	if (initProcs(PROCS_PREFIX_PATH, CONF_PATH) != 0) {
		fprintf(stderr, "Error starting platform.\n");
		exit(1);
	}

	timestamp_id = EXEC_START_COMPILATION;
	if (hookEvent(EXECUTOR, TIMESTAMP, BPatch_callBefore, &timestamp_id, "load_code") != 0) {
		fprintf(stderr, "Error adding event into main");
		exit(1);
	}

	timestamp_id = EXEC_END_COMPILATION;
	if (hookEvent(EXECUTOR, TIMESTAMP, BPatch_callAfter, &timestamp_id, "load_code") != 0) {
		fprintf(stderr, "Error adding event into main");
		exit(1);
	}

	if (startTest(10) != 0) {
		fprintf(stderr, "Error starting test");
		exit(1);
	}

	sleep(2);

	if (!fork()) {

		int i;
		for (i = 0; i < 30; i++) {
			bundle_s *b = createBundle();
			char *b_name;
			storeBundleIntoQueue("/home/xeri/projects/adtn/root/var/lib/adtn", b, &b_name);
			executeCode("/home/xeri/projects/adtn/root/var/lib/adtn", b_name);
		}
		_exit(0);
	}

	waitTestEnd();

	// sleep(30);

	// Show events
	event_t *event = testEnv.eventQueue;
	while (event != NULL) {
		printf("Event type: %d, data: %d, timestamp: %lu secs %lu nsecs\n", event->event, *event->data, event->timestamp.tv_sec, event->timestamp.tv_nsec);
		//printf("Event type: %d, data: %d, timestamp: %lu secs %lu nsecs\n", event->next->event, *event->next->data, event->next->timestamp.tv_sec, event->next->timestamp.tv_nsec);
		//printf("time: %lf\n", diff_time(&event->timestamp, &event->next->timestamp)/1.0e9);
		//event = event->next->next;
		event = event->next;

	}


	return 0;
}