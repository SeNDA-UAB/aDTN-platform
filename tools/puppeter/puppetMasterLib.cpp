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
#include <boost/bind.hpp>

// C
#include <sys/mman.h>   /* shm_open, mmap..*/
#include <sys/stat.h>   /* For mode constants */
#include <fcntl.h>      /* For O_* constants */
#include <string.h>     /* strcmp */
#include <errno.h>

#ifndef DYNINSTAPI_RT_LIB
#define DYNINSTAPI_RT_LIB "/usr/local/lib64/libdyninstAPI_RT.so"
#endif

#ifndef PUPPET_LIB_PATH
#define PUPPET_LIB_PATH "/home/xeri/projects/adtn/repo/aDTN-platform/build/tools/puppeter/libpuppetLib.so"
#endif

using namespace std;

// Global BPatch instance
static BPatch bpatch;

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
	puppetCtx_t *puppetCtx = new puppetCtx_t();

	puppetCtx->name = puppetName;
	puppetCtx->cmd = puppetCmd;
	puppetCtx->pos = puppets.size();

	puppets.insert(make_pair(puppetName, puppetCtx));

	return 0;
}

bool cmpPuppet(puppetCtx_t *a, puppetCtx_t *b)
{
	return a->pos < b->pos;
}

int puppeteerCtx::initPuppets()
{
	// Sort puppets by position
	vector<puppetCtx_t *> v;
	v.reserve(puppets.size());
	transform( puppets.begin(), puppets.end(),
	           back_inserter(v), boost::bind(&map<string, puppetCtx_t *>::value_type::second, _1) );
	sort(v.begin(), v.end(), cmpPuppet);

	// Init puppets in insertion order
	for (vector<puppetCtx_t *>::iterator it = v.begin();
	     it != v.end();
	     ++it) {

		// Prepare cmd and argv array
		vector< string > cmdVec;
		boost::split(cmdVec, (*it)->cmd, boost::algorithm::is_any_of(" "));

		const char **argv = new const char *[cmdVec.size() + 1];
		vector<string>::iterator it_argv = cmdVec.begin();

		int i = 0;
		for (; it_argv != cmdVec.end(); ++it_argv) {
			argv[i] = it_argv->c_str();
			i++;
		}
		argv[i] = NULL;

		(*it)->handle = bpatch.processCreate(cmdVec[0].c_str(), argv);

		// TODO
		//delete argv;
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

		puppetCtx->shmCtx.initialized = 1;
	}
}
void puppeteerCtx::getFunctions(BPatch_image *appImage, const string funcName, vector<BPatch_function *> &functions)
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
		getFunctions(appImage, "puppeteerEndEvent", endEvent);

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
	app->insertSnippet(eventCall, *points, when, BPatch_lastSnippet);

	//endEvent
	BPatch_funcCallExpr puppeteerEndEventCall(*(endEvent[0]), noArgs);
	app->insertSnippet(puppeteerEndEventCall, *points, when, BPatch_lastSnippet);

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
			shmEvent->eventBufferStart = (shmEvent->eventBufferStart + 1 ) % shmEvent->eventBufferSize;
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
	int sig;
	siginfo_t si;
	sigset_t blockedSigs = {{0}};
	struct timespec timeout = {0};
	timeout.tv_sec = 60;

	// sigaddset(&blockedSigs, SIGINT);
	// sigaddset(&blockedSigs, SIGTERM);

	// do {
	//  sig = sigtimedwait(&blockedSigs, &si, &timeout);
	// } while (sig < 0 && errno == EINTR);

	sleep(60);

	vector<puppetCtx_t *> v;
	v.reserve(puppets.size());
	transform( puppets.begin(), puppets.end(),
	           back_inserter(v), boost::bind(&map<string, puppetCtx_t *>::value_type::second, _1) );
	sort(v.begin(), v.end(), cmpPuppet);

	// Init puppets in insertion order
	for (vector<puppetCtx_t *>::reverse_iterator it = v.rbegin();
	     it != v.rend();
	     ++it) {
		BPatch_process *appProc = dynamic_cast<BPatch_process *>((*it)->handle);
		//appProc->terminateExecution();
		kill(appProc->getPid(), SIGINT);
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
	// Block all signals so they will be handled by the endPuppetsThread
	// sigset_t blockedSigs = {{0}};
	// sigaddset(&blockedSigs, SIGINT);
	// sigaddset(&blockedSigs, SIGTERM);
	// sigprocmask(SIG_BLOCK, &blockedSigs, NULL);

	for (map<string, puppetCtx_t *>::iterator it = puppets.begin(); it != puppets.end(); ++it) {
		if (it->second->shmCtx.initialized)
			launchEventListenerThread(it->second);

		BPatch_process *appProc = dynamic_cast<BPatch_process *>(it->second->handle);
		appProc->continueExecution();
	}

	launchEndPuppetsThread(60);

	return 0;
}

int puppeteerCtx::waitTestEnd()
{
	BPatch_process *appProc;

	// Wait until all processes have been terminated
	for (;;) {
		int all_terminated = 1;
		for (map<string, puppetCtx_t *>::iterator it = puppets.begin(); it != puppets.end(); ++it) {
			appProc = dynamic_cast<BPatch_process *>(it->second->handle);
			if (!appProc->isTerminated()) {
				all_terminated = 0;
				break;
			}
		}

		if (all_terminated) {
			break;
		} else {
			bpatch.waitForStatusChange();
		}
	}

	return 0;
}

bool cmpEvent(puppeteerEvent_t &a, puppeteerEvent_t &b)
{
	if (a.timestamp.tv_sec != b.timestamp.tv_sec) {
		return a.timestamp.tv_sec < b.timestamp.tv_sec;
	} else {
		return a.timestamp.tv_nsec < b.timestamp.tv_nsec;
	}
}

int puppeteerCtx::getEvents(list<puppeteerEvent_t> &eventList)
{
	for (map<string, puppetCtx_t *>::iterator it = puppets.begin(); it != puppets.end(); ++it) {
		eventList.merge(it->second->eventsList, cmpEvent);
	}

	return 0;
}

int puppeteerCtx::printEvents()
{
	// Merge events
	list<puppeteerEvent_t> eventList;
	getEvents(eventList);

	for (list<puppeteerEvent_t>::iterator it_ev = eventList.begin(); it_ev != eventList.end(); ++it_ev) {
		cout << it_ev->eventId << ": " << it_ev->timestamp.tv_sec << "." << it_ev->timestamp.tv_nsec << "s " << it_ev->data << endl;
	}

	return 0;
}

double diff_time(const struct timespec &start, const struct timespec &end)
{
	return (double)(end.tv_sec - start.tv_sec) * 1.0e9 + (double)(end.tv_nsec - start.tv_nsec);
}

double puppeteerCtx::getDiffTimestamp(const puppeteerEvent_t &a, const puppeteerEvent_t &b)
{
	return diff_time(a.timestamp, b.timestamp);
}

int puppeteerCtx::countEvents(list<puppeteerEvent_t> &eventList, const char data[MAX_EVENT_DATA])
{
	int count = 0;
	for (list<puppeteerEvent_t>::iterator it_ev = eventList.begin(); it_ev != eventList.end(); ++it_ev) {
		if (strcmp(it_ev->data, data) == 0)
			count++;
	}

	return count;
}

const puppeteerEvent_t *puppeteerCtx::findNextEvent(list<puppeteerEvent_t>::const_iterator &start, const list<puppeteerEvent_t>::const_iterator &end, const char eventData[MAX_EVENT_DATA])
{
	for (; start != end; ++start) {
		if (strcmp(start->data, eventData) == 0) {
			return &(*start);
		}
	}
	return NULL;
}

int puppeteerCtx::pairEvents(list<puppeteerEvent_t> &eventList, const char a[MAX_EVENT_DATA], const char b[MAX_EVENT_DATA], list< pair<const puppeteerEvent_t *, const puppeteerEvent_t *> > &pairedList)
{
	int count = 0;

	list<puppeteerEvent_t>::const_iterator lastEventB = eventList.cbegin();
	for (list<puppeteerEvent_t>::iterator it_ev = eventList.begin(); it_ev != eventList.end(); ++it_ev) {
		// Find a
		if (strcmp(it_ev->data, a) == 0) {
			// Find next b
			const puppeteerEvent_t *eventB = findNextEvent(lastEventB, eventList.cend(), b);
			++lastEventB;
			pair<const puppeteerEvent_t *, const puppeteerEvent_t *> new_pair = make_pair(&(*it_ev), eventB);
			pairedList.push_back(new_pair);
			count++;
		}
	}

	return count;
}

int puppeteerCtx::getPairStats(list< pair<const puppeteerEvent_t *, const puppeteerEvent_t *> > &pairedList, double &max, double &min, double &avg)
{
	int count = 0;

	min = -1;
	max = -1;
	avg = 0;

	for (list< pair<const puppeteerEvent_t *, const puppeteerEvent_t *> >::const_iterator it = pairedList.cbegin(); it != pairedList.cend(); ++it) {
		double diff = diff_time(it->first->timestamp, it->second->timestamp) ;

		// Initialization
		if (min == -1)
			min = diff;
		if (max == -1)
			max = diff;

		// Update
		if (diff < min)
			min = diff;
		else if (diff > max)
			max = diff;
		avg += diff;

		count++;
	}

	avg = avg / count;

}