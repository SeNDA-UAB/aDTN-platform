// Dyninst
#include "BPatch.h"
#include "BPatch_addressSpace.h"
#include "BPatch_process.h"
#include "BPatch_binaryEdit.h"
#include "BPatch_function.h"
#include "BPatch_point.h"
#include "BPatch_flowGraph.h"

// Pupeteer
#include "include/puppetMasterLib.h"

// CPP
#include <iostream>
#include <map>

// Boost
#include <boost/algorithm/string.hpp>

// C
#include <sys/mman.h> /* shm_open, mmap..*/
#include <sys/stat.h> /* For mode constants */
#include <fcntl.h> /* For O_* constants */


#ifndef DYNINSTAPI_RT_LIB
#define DYNINSTAPI_RT_LIB "/usr/local/lib64/libdyninstAPI_RT.so"
#endif

#ifndef PUPPET_LIB_PATH
#define PUPPET_LIB_PATH "/usr/local/lib64/puppetLib.so"
#endif

using namespace std;

// Global BPatch instance
static BPatch bpatch;

typedef struct puppetShmCtx_s {
	int initialized;
	int fd;
	shmEventCtx_t *shm;
} puppetShmCtx_t;

typedef struct puppetCtx_s {
	string name;
	string cmd;
	BPatch_addressSpace *handle;
	BPatch_module *puppetLib;
	puppetShmCtx_t shmCtx;
} puppetCtx_t;

class puppeteerCtx
{

private:
	map<string, puppetCtx_t *> puppets;
	void preparePuppetMemory(puppetCtx_t &puppetCtx);
	vector<BPatch_point *> *getEntryPoints(BPatch_image *appImage, const string funcName, puppeteerEventLoc_e loc);

public:
	puppeteerCtx();
	int addPuppet(string puppetName, string puppetCmd);
	int initPuppets();
	int addEvent(const string puppetName, const string function,
	             const puppeteerEventLoc_e loc,
	             const puppeteerEvent_e eventId,
	             const uint8_t data[MAX_EVENT_DATA]);
	int startPuppets();
};

puppeteerCtx::puppeteerCtx()
{
	setenv("DYNINSTAPI_RT_LIB", DYNINSTAPI_RT_LIB, 0);

	/* Optimizations */
	// Turn on or off trampoline recursion. By default, any snippets invoked while another snippet is active will not be executed.
	bpatch.setTrampRecursive(true);
	// Turn on or off floating point saves.
	bpatch.setSaveFPR(false);
	/**/
}

int puppeteerCtx::addPuppet(const string puppetName, const string puppetCmd)
{
	puppetCtx_t *puppetCtx = new puppetCtx_t;
	puppetCtx->name = puppetName;

	puppets.insert(make_pair(puppetName, puppetCtx));

	return 0;
}

int puppeteerCtx::initPuppets()
{
	map<string, puppetCtx_t *>::iterator it = puppets.begin();
	for (; it != puppets.end(); ++it) {

		// Prepare cmd and argv array
		vector< string > cmdVec;
		boost::split(cmdVec, it->second->cmd, " ");

		const char **argv = new const char *[cmdVec.size()];
		vector<string>::iterator it_argv = cmdVec.begin();

		for (++it_argv; it_argv != cmdVec.end(); ++it_argv) {
			argv[it_argv - cmdVec.begin()] = it_argv->c_str();
		}

		it->second->handle = bpatch.processCreate(cmdVec[0].c_str(), argv);

		delete argv;
	}

	return 0;
}

void puppeteerCtx::preparePuppetMemory(puppetCtx_t &puppetCtx)
{
	if (!puppetCtx.shmCtx.initialized) {
		BPatch_addressSpace *app = puppetCtx.handle;
		BPatch_image *appImage = app->getImage();
		BPatch_process *appProc = dynamic_cast<BPatch_process *>(app);

		/* Initializes and map shared memory region into puppet master proc. */
		char shmName[32];
		snprintf(shmName, sizeof(shmName), "/%d-%d", getpid(), appProc->getPid());
		puppetCtx.shmCtx.fd = shm_open(shmName, O_RDWR | O_CREAT | O_EXCL, S_IRWXU | S_IRWXG);
		ftruncate(puppetCtx.shmCtx.fd, sizeof(shmEventCtx_t));
		puppetCtx.shmCtx.shm = (shmEventCtx_t *) mmap(NULL, sizeof(shmEventCtx_t), PROT_READ | PROT_WRITE, MAP_SHARED, puppetCtx.shmCtx.fd, 0);
		bzero(puppetCtx.shmCtx.shm, sizeof(shmEventCtx_t));

		// Init mutex
		pthread_mutexattr_t event_mAttr;
		pthread_mutexattr_init(&event_mAttr);
		pthread_mutexattr_setpshared(&event_mAttr, PTHREAD_PROCESS_SHARED);
		pthread_mutex_init(&puppetCtx.shmCtx.shm->event_m, &event_mAttr);

		// Init cond
		pthread_condattr_t event_cAttr;
		pthread_condattr_init(&event_cAttr);
		pthread_condattr_setpshared(&event_cAttr, PTHREAD_PROCESS_SHARED);
		pthread_cond_init(&puppetCtx.shmCtx.shm->event_c, &event_cAttr);

		/**/

		/* Map shared memory into puppet proc. First we load puppetLib, then we execute:
		 * puppeteerInitShm(shmName);
		 */
		puppetCtx.puppetLib = (BPatch_module *)app->loadLibrary(PUPPET_LIB_PATH);

		// puppeteerInitShm(shmName);
		vector<BPatch_function *>puppeteerInitShmFuncs;
		appImage->findFunction("puppeteerInitShm", puppeteerInitShmFuncs);
		if (puppeteerInitShmFuncs.size() == 0 || puppeteerInitShmFuncs.size() > 1)
			throw "More than one or no puppeteerInitShm function found. Something went wrong.";

		// Prepare args
		vector<BPatch_snippet *> puppeteerInitShmArgs;
		BPatch_constExpr puppeterInitShmName(shmName);

		puppeteerInitShmArgs.push_back(&puppeterInitShmName);

		// Create procedure call
		BPatch_funcCallExpr puppeteerInitShmCall(*puppeteerInitShmFuncs[0], puppeteerInitShmArgs);

		// Execute shm_open and get returned fd
		int ret = (uintptr_t) appProc->oneTimeCode(puppeteerInitShmCall);
		if (ret != 0)
			throw "Error initializing puppet shared memory";
		/**/
	}
}

vector<BPatch_point *> *getEntryPoints(BPatch_image *appImage, const string funcName, puppeteerEventLoc_e loc)
{
	vector<BPatch_function *> functions;
	vector<BPatch_point *> *points = NULL;

	// Find function entry points
	appImage->findFunction(funcName.c_str(), functions);
	if (functions.size() == 0) {
		throw "Can't find function " + funcName;
	} else if (functions.size() > 1) {
		cout << "Warning more than one function " + funcName + " found" << endl;
	}
	
	if (loc == puppeteerEventLocBefore)
		points = functions[0]->findPoint(BPatch_entry);
	else if (loc == puppeteerEventLocAfter)
		points = functions[0]->findPoint(BPatch_entry);
	else
		throw "Supplied loc not implementetd";

	if (points->size() == 0) {
		throw "Can't find function " + funcName + " entry points";
	} else if (points->size() > 1) {
		cout << "Warning more than one entry point in function " + funcName + " found" << endl;
	}

	return points;
}

int puppeteerCtx::addEvent(const string puppetName, const string funcName,
                           const puppeteerEventLoc_e loc,
                           const puppeteerEvent_e eventId,
                           const uint8_t data[MAX_EVENT_DATA])
{
	// Get puppet ctx
	puppetCtx_t *puppetCtx = NULL;
	map<string, puppetCtx_t *>::iterator it = puppets.find(puppetName);
	if (it != puppets.end()) {
		puppetCtx = it->second;
	} else {
		throw "puppetCtx not found for puppet " + puppetName;
	}

	// Initialize puppet shm if needed
	try {
		preparePuppetMemory(*puppetCtx);
	} catch (char const *err) {
		cerr << err << endl;
		throw "Can't initialize puppet " + puppetName + " memory";
	}

	// Find hook points
	BPatch_addressSpace *app = puppetCtx->handle;
	BPatch_image *appImage = app->getImage();
	BPatch_process *appProc = dynamic_cast<BPatch_process *>(app);

	vector<BPatch_function *> targetFuncs;
	appImage->findFunction(funcName.c_str(), targetFuncs);

	if (targetFuncs.size() == 0){
		throw "No functions " + funcName + " found in puppet " + puppetName;
	} else if (targetFuncs.size() > 1){
		cout << "Warning: More than one function " + funcName + " found in puppet " + puppetName;
	}

	// Insert hooks
	try {
		vector<BPatch_point *> *points = getEntryPoints(appImage, funcName, loc);
	} catch (char const *err) {
		cerr << err << endl;
		throw "Can't get function " + puppetName + " entry points";
	} 

	BPatch_callWhen when;
	if (loc == puppeteerEventLocBefore)
		when = BPatch_callBefore;
	else if (loc == puppeteerEventLocAfter)
		when = BPatch_callAfter;

	



}

