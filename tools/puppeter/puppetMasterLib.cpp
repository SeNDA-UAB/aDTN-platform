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
#include <boost/fusion/container/list.hpp>
#include <boost/fusion/include/list.hpp>
#include <boost/fusion/container/list/list_fwd.hpp>
#include <boost/fusion/include/list_fwd.hpp>
#include <boost/algorithm/string.hpp>

// C
#include <sys/mman.h>   /* shm_open, mmap..*/
#include <sys/stat.h>   /* For mode constants */
#include <fcntl.h>      /* For O_* constants */


#ifndef DYNINSTAPI_RT_LIB
#define DYNINSTAPI_RT_LIB "/usr/local/lib64/libdyninstAPI_RT.so"
#endif

#ifndef PUPPET_LIB_PATH
#define PUPPET_LIB_PATH "/usr/local/lib64/puppetLib.so"
#endif

using namespace std;
//using namespace boost::intrusive; // For slist

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
	pthread_t eventListenerThreadId;
	list<puppeteerEvent_t> eventsList;
} puppetCtx_t;

class puppeteerCtx
{
private:
	map<string, puppetCtx_t *> puppets;

	void preparePuppetMemory(puppetCtx_t *puppetCtx);
	void getEntryPoints(BPatch_image *appImage, const string funcName, puppeteerEventLoc_e loc, vector<BPatch_point *> **points);
	void getFunctions(BPatch_image *appImage, const string funcName, vector<BPatch_function *> functions);

	static void *eventListenerThread(void *puppetCtx_p);
	void launchEventListenerThread(puppetCtx_t *puppetCtx);
	static void *endPuppetsThread(void *puppets);
	void launchEndPuppetsThread(int secs);

public:
	puppeteerCtx();
	int addPuppet(string puppetName, string puppetCmd);
	int initPuppets();
	// data must be NULL terminated
	int addEvent(const string puppetName, const string function,
	             const puppeteerEventLoc_e loc,
	             const puppeteerEvent_e eventId,
	             const char data[MAX_EVENT_DATA]);
	int startTest(const int secs);
	int waitTestEnd();
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
	for (map<string, puppetCtx_t *>::iterator it = puppets.begin();
	     it != puppets.end();
	     ++it) {

		// Prepare cmd and argv array
		vector< string > cmdVec;
		boost::split(cmdVec, it->second->cmd, boost::algorithm::is_any_of(" "));

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

void puppeteerCtx::preparePuppetMemory(puppetCtx_t *puppetCtx)
{
	if (!puppetCtx->shmCtx.initialized) {
		BPatch_addressSpace *app = puppetCtx->handle;
		BPatch_image *appImage = app->getImage();
		BPatch_process *appProc = dynamic_cast<BPatch_process *>(app);

		/* Initializes and map shared memory region into puppet master proc. */
		char shmName[32];
		snprintf(shmName, sizeof(shmName), "/%d-%d", getpid(), appProc->getPid());
		puppetCtx->shmCtx.fd = shm_open(shmName, O_RDWR | O_CREAT | O_EXCL, S_IRWXU | S_IRWXG);
		ftruncate(puppetCtx->shmCtx.fd, sizeof(shmEventCtx_t));
		puppetCtx->shmCtx.shm = (shmEventCtx_t *) mmap(NULL, sizeof(shmEventCtx_t), PROT_READ | PROT_WRITE, MAP_SHARED, puppetCtx->shmCtx.fd, 0);
		bzero(puppetCtx->shmCtx.shm, sizeof(shmEventCtx_t));

		// Init mutex
		pthread_mutexattr_t event_mAttr;
		pthread_mutexattr_init(&event_mAttr);
		pthread_mutexattr_setpshared(&event_mAttr, PTHREAD_PROCESS_SHARED);
		pthread_mutex_init(&puppetCtx->shmCtx.shm->event_m, &event_mAttr);

		// Init cond
		pthread_condattr_t event_cAttr;
		pthread_condattr_init(&event_cAttr);
		pthread_condattr_setpshared(&event_cAttr, PTHREAD_PROCESS_SHARED);
		pthread_cond_init(&puppetCtx->shmCtx.shm->event_c, &event_cAttr);
		/**/

		/* Map shared memory into puppet proc. First we load puppetLib, then we execute:
		 * puppeteerInitShm(shmName);
		 */
		puppetCtx->puppetLib = (BPatch_module *)app->loadLibrary(PUPPET_LIB_PATH);

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

		//puppetCtx->eventsList = slist<puppeteerEvent_t>(2);
	}
}
void puppeteerCtx::getFunctions(BPatch_image *appImage, const string funcName, vector<BPatch_function *> functions)
{
	// Find function entry points
	appImage->findFunction(funcName.c_str(), functions);
	if (functions.size() == 0) {
		throw "Can't find function " + funcName;
	} else if (functions.size() > 1) {
		cout << "Warning more than one function " + funcName + " found" << endl;
	}
}

void puppeteerCtx::getEntryPoints(BPatch_image *appImage, const string funcName, puppeteerEventLoc_e loc, vector<BPatch_point *> **points)
{
	vector<BPatch_function *> functions;

	getFunctions(appImage, funcName, functions);

	if (loc == puppeteerEventLocBefore)
		*points = functions[0]->findPoint(BPatch_entry);
	else if (loc == puppeteerEventLocAfter)
		*points = functions[0]->findPoint(BPatch_exit);
	else
		throw "Supplied loc not implementetd";

	if ((*points)->size() == 0) {
		throw "Can't find function " + funcName + " entry points";
	} else if ((*points)->size() > 1) {
		cout << "Warning more than one entry point in function " + funcName + " found" << endl;
	}
}

int puppeteerCtx::addEvent(const string puppetName, const string funcName,
                           const puppeteerEventLoc_e loc,
                           const puppeteerEvent_e eventId,
                           const char data[MAX_EVENT_DATA])
{
	/* prepare puppet */
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
		preparePuppetMemory(puppetCtx);
	} catch (char const *err) {
		cerr << err << endl;
		throw "Can't initialize puppet " + puppetName + " memory";
	}
	/**/

	/* Insert hooks */
	BPatch_addressSpace *app = puppetCtx->handle;
	BPatch_image *appImage = app->getImage();

	// Get function entry points
	vector<BPatch_point *> *points = NULL;
	try {
		getEntryPoints(appImage, funcName, loc, &points);
	} catch (char const *err) {
		cerr << err << endl;
		throw "Can't get function " + puppetName + " entry points";
	}

	BPatch_callWhen when;
	if (loc == puppeteerEventLocBefore)
		when = BPatch_callBefore;
	else if (loc == puppeteerEventLocAfter)
		when = BPatch_callAfter;
	else
		throw "Supplied loc not implementetd";

	// Get event hooks
	vector<BPatch_function *> startEvent, event, endEvent;
	try {
		getFunctions(appImage, "puppeteerStartEvent", startEvent);
		switch (eventId) {
		case puppeteerEventSimpleId:
			getFunctions(appImage, "puppeteerEventSimple", event);
			break;
		}
		getFunctions(appImage, "puppeterEndEvent", endEvent);

	} catch (char const *err) {
		cerr << err << endl;
		throw "Can't get puppeter events hooks.";
	}

	// Hook function
	vector<BPatch_snippet *> noArgs, eventArgs;
	app->beginInsertionSet();

	// startEvent
	BPatch_funcCallExpr puppeteerStartEventCall(*(startEvent[0]), noArgs);
	app->insertSnippet(puppeteerStartEventCall, *points, when, BPatch_lastSnippet);

	// event
	BPatch_constExpr eventDataArg(data);
	eventArgs.push_back(&eventDataArg);
	BPatch_funcCallExpr eventCall(*(event[0]), eventArgs);

	//endEvent
	BPatch_funcCallExpr puppeteerEndEventCall(*(endEvent[0]), noArgs);

	app->finalizeInsertionSet(true);
	/**/

	return 0;
}

void *puppeteerCtx::eventListenerThread(void *puppetCtx_p)
{
	shmEventCtx_t *shmEvent = ((puppetCtx_t *)puppetCtx_p)->shmCtx.shm;

	do {
		pthread_mutex_lock((pthread_mutex_t *)&shmEvent->event_m);

		// Wait until there is a new event
		while (shmEvent->eventBufferStart == shmEvent->eventBufferEnd)
			pthread_cond_wait((pthread_cond_t *)&shmEvent->event_c, (pthread_mutex_t *)&shmEvent->event_m);

		// Process all new events
		while (shmEvent->eventBufferStart != shmEvent->eventBufferEnd) {
			// Get event
			puppeteerEvent_t *event = &shmEvent->eventBuffer[shmEvent->eventBufferStart];

			// Store event
			((puppetCtx_t *)puppetCtx_p)->eventsList.push_back(*event);

			//Next event
			shmEvent->eventBufferStart = shmEvent->eventBufferStart % shmEvent->eventBufferSize;
		}

		pthread_mutex_unlock((pthread_mutex_t *)&shmEvent->event_m);

	} while (1);
}

void puppeteerCtx::launchEventListenerThread(puppetCtx_t *puppetCtx)
{
	pthread_create(&puppetCtx->eventListenerThreadId, NULL, eventListenerThread, puppetCtx);
}

void *puppeteerCtx::endPuppetsThread(void *puppets_p)
{
	map<string, puppetCtx_t *> puppets = *(map<string, puppetCtx_t *> *) puppets_p;

	// TODO: get secs from launch thread call.
	sleep(10);

	for (map<string, puppetCtx_t *>::iterator it = puppets.begin(); it != puppets.end(); ++it) {
		BPatch_process *appProc = dynamic_cast<BPatch_process *>(it->second->handle);
		appProc->terminateExecution();
	}

	return NULL;
}

void puppeteerCtx::launchEndPuppetsThread(int secs)
{
	pthread_t endPuppetsThreadId;

	pthread_create(&endPuppetsThreadId, NULL, endPuppetsThread, &puppets);
}

int puppeteerCtx::startTest(const int secs)
{
	for (map<string, puppetCtx_t *>::iterator it = puppets.begin(); it != puppets.end(); ++it) {
		if (it->second->shmCtx.initialized)
			launchEventListenerThread(it->second);

		BPatch_process *appProc = dynamic_cast<BPatch_process *>(it->second->handle);
		appProc->continueExecution();
	}

	return 0;
}