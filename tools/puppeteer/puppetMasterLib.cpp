// Dyninst
#include "BPatch.h"
#include "BPatch_addressSpace.h"
#include "BPatch_process.h"
#include "BPatch_binaryEdit.h"
#include "BPatch_function.h"
#include "BPatch_point.h"
#include "BPatch_flowGraph.h"

// Boost
#include <boost/algorithm/string.hpp>

// CPP
#include <chrono>
#include <thread>

// C
#include <sys/mman.h>   /* shm_open, mmap..*/
#include <sys/stat.h>   /* For mode constants */
#include <fcntl.h>      /* For O_* constants */

#include "include/puppetMasterLib.hpp"

/**/


/* Exceptions */
class ErrorInitializingPuppetMemory: public logic_error
{
public:
	ErrorInitializingPuppetMemory(): logic_error("Can't initialize puppet memory.") {};
	ErrorInitializingPuppetMemory(string errMsg): logic_error(errMsg) {};
};

class NoEntryPoint: public logic_error
{
public:
	NoEntryPoint(): logic_error("Can't find entry point.") {};
	NoEntryPoint(string errMsg): logic_error(errMsg) {};
};

/**/

// http://herbsutter.com/gotw/_100/
class puppeteerCtx::impl
{
private:
	struct PuppetShmCtx {
		int initialized;
		int fd;
		shmEventCtx_t *shm;
	};

	struct PuppetCtx {
		string name;
		string cmd;
		BPatch_addressSpace *handle;
		BPatch_module *puppetLib;
		PuppetShmCtx shmCtx;
		vector<puppeteerEvent_t> eventsList;
	};

	bool puppetsInitialized;
	vector<PuppetCtx> puppets;
	multimap<int, function<void()>> actions;

	/* Launched as threads */
	void eventListener(PuppetCtx &puppet);
	void launchActions();
	/**/

	vector<PuppetCtx>::iterator findPuppet(const string puppetName);
	void preparePuppetMemory(PuppetCtx &p);
	void getFunctions(  BPatch_image *appImage,
	                    const string function,
	                    const bool multiple,
	                    vector<BPatch_function *> &functions    );
	vector<BPatch_point *> *getEntryPoints( BPatch_image *appImage,
	                                        const string function,
	                                        const bool multiple,
	                                        const puppeteerEventLoc loc );
	void waitTestEnd(const int secs);
public:
	void addPuppet(string puppetName, string puppetCmd);
	void initPuppets();	
	void addEvent(  const string puppetName,
	                const string function,
	                const bool multiple,
	                const puppeteerEventLoc loc,
	                const puppeteerEvent_e eventId,
	                const char data[MAX_EVENT_DATA]   );
	void addAction(const int delay, const function<void()> a);
	void startTest(const int secs, const bool endTest, const bool forceEnd);
	void endTest(const int secs, const bool force);
	vector<puppeteerEvent_t> getEvents(const string puppetName);
};
/**/

// Global BPatch instance
static BPatch bpatch;

/* Private members */
vector<puppeteerCtx::impl::PuppetCtx>::iterator
puppeteerCtx::impl::findPuppet(const string puppetName)
{
	for (vector<PuppetCtx>::iterator it = puppets.begin(); it != puppets.end(); ++it) {
		if (it->name == puppetName)
			return it;
	}

	throw PuppetNotFound("Puppet " + puppetName + " not found in puppeteer context.");
}

void
puppeteerCtx::impl::preparePuppetMemory(PuppetCtx &p)
{
	BPatch_addressSpace *app = p.handle;
	BPatch_image *appImage = app->getImage();
	BPatch_process *appProc = dynamic_cast<BPatch_process *>(app);

	/* Initializes and map shared memory region into puppet master proc. */
	char shmName[32];
	snprintf(shmName, sizeof(shmName), "/%d-%d", getpid(), appProc->getPid());
	p.shmCtx.fd = shm_open(shmName, O_RDWR | O_CREAT | O_EXCL, S_IRWXU | S_IRWXG);
	ftruncate(p.shmCtx.fd, sizeof(shmEventCtx_t));
	p.shmCtx.shm = (shmEventCtx_t *) mmap(NULL, sizeof(shmEventCtx_t), PROT_READ | PROT_WRITE, MAP_SHARED, p.shmCtx.fd, 0);
	bzero(p.shmCtx.shm, sizeof(shmEventCtx_t));

	// Init mutex
	pthread_mutexattr_t event_mAttr;
	pthread_mutexattr_init(&event_mAttr);
	pthread_mutexattr_setpshared(&event_mAttr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&p.shmCtx.shm->event_m, &event_mAttr);

	// Init cond
	pthread_condattr_t event_cAttr;
	pthread_condattr_init(&event_cAttr);
	pthread_condattr_setpshared(&event_cAttr, PTHREAD_PROCESS_SHARED);
	pthread_cond_init(&p.shmCtx.shm->event_c, &event_cAttr);
	/**/

	/* Map shared memory into puppet proc. First we load puppetLib, then we execute:
	 * puppeteerInitShm(shmName);
	 */
	p.puppetLib = (BPatch_module *)app->loadLibrary(puppet_lib_path);

	// puppeteerInitShm(shmName);
	vector<BPatch_function *>puppeteerInitShmFuncs;
	appImage->findFunction("puppeteerInitShm", puppeteerInitShmFuncs);
	if (puppeteerInitShmFuncs.size() == 0 || puppeteerInitShmFuncs.size() > 1)
		throw ErrorInitializingPuppetMemory("More than one puppeteerInitShm function found in " + p.name + " memory space. Something is wrong.");

	// Prepare args
	vector<BPatch_snippet *> puppeteerInitShmArgs;
	BPatch_constExpr puppeterInitShmName(shmName);

	puppeteerInitShmArgs.push_back(&puppeterInitShmName);

	// Create procedure call
	BPatch_funcCallExpr puppeteerInitShmCall(*puppeteerInitShmFuncs[0], puppeteerInitShmArgs);

	// Execute shm_open and get returned fd
	int ret = (uintptr_t) appProc->oneTimeCode(puppeteerInitShmCall);
	if (ret != 0)
		throw ErrorInitializingPuppetMemory("Puppet " + p.name + " puppeteerInitShm call failed.");
	/**/

	p.shmCtx.initialized = 1;
}

void
puppeteerCtx::impl::getFunctions(   BPatch_image *appImage,
                                    const string function,
                                    const bool multiple,
                                    vector<BPatch_function *> &functions )
{
	appImage->findFunction(function.c_str(), functions);
	if (functions.size() == 0) {
		throw FunctionNotFound("Function " + function + "not found.");
	} else if (functions.size() > 1) {
		if (!multiple)
			throw MultipleHooks("Found more than one function " + function);
	}
}

vector<BPatch_point *> *
puppeteerCtx::impl::getEntryPoints( BPatch_image *appImage,
                                    const string function,
                                    const bool multiple,
                                    const puppeteerEventLoc loc )
{
	vector<BPatch_function *> functions;
	getFunctions(appImage, function, multiple, functions);

	vector<BPatch_point *> *points = NULL, *new_points = NULL;
	if (loc == puppeteerEventLoc::puppeteerEventLocBefore) {
		for (auto &f : functions) {
			if (points == NULL) {
				points = f->findPoint(BPatch_entry);
			} else {
				new_points = f->findPoint(BPatch_entry);
				points->insert(points->end(), new_points->begin(), new_points->end());
				delete new_points;
			}
		}
	} else if (loc == puppeteerEventLoc::puppeteerEventLocAfter) {
		for (auto &f : functions) {
			if (points == NULL) {
				points = f->findPoint(BPatch_exit);
			} else {
				new_points = f->findPoint(BPatch_exit);
				points->insert(points->end(), new_points->begin(), new_points->end());
				delete new_points;
			}
		}
	} else {
		throw InvalidArgument("Supplied loc not implementetd");
	}

	if (points == NULL || points->size() == 0) {
		throw NoEntryPoint("Can't find function " + function + " entry points");
	} else if (points->size() > 1) {
		if (!multiple)
			throw MultipleHooks("Found more than one entry point for function " + function);
	}

	return points;
}


void
puppeteerCtx::impl::addPuppet(  const string puppetName,
                                const string puppetCmd  )
{
	PuppetCtx newPuppet = {};

	newPuppet.name = puppetName;
	newPuppet.cmd = puppetCmd;


	puppets.push_back(newPuppet);
}

void
puppeteerCtx::impl::initPuppets()
{

	for (auto &p : puppets) {

		// Prepare cmd and argv array
		vector< string > cmdVec;
		boost::split(cmdVec, p.cmd, boost::algorithm::is_any_of(" "));

		const char **argv = new const char *[cmdVec.size() + 1];

		int i = 0;
		for (   vector<string>::iterator it_argv = cmdVec.begin();
		        it_argv != cmdVec.end();
		        ++it_argv   ) {
			argv[i] = it_argv->c_str();
			i++;
		}
		argv[i] = NULL;

		p.handle = bpatch.processCreate(cmdVec[0].c_str(), argv);

		delete argv;
	}

	puppetsInitialized = true;

}

void
puppeteerCtx::impl::addEvent(  const string puppetName,
                               const string function,
                               const bool multiple,
                               const puppeteerEventLoc loc,
                               const puppeteerEvent_e eventId,
                               const char data[MAX_EVENT_DATA] )
{
	if (!puppetsInitialized)
		initPuppets();

	vector<PuppetCtx>::iterator it = findPuppet(puppetName);

	/* Initialize puppet */
	PuppetCtx &p = *it;
	if (!p.shmCtx.initialized) {
		try {
			preparePuppetMemory(p);
		} catch (ErrorInitializingPuppetMemory &e) {
			throw PuppeteerError(e.what());
		};
	}
	/**/

	/* Insert hooks */
	BPatch_addressSpace *app = p.handle;
	BPatch_image *appImage = app->getImage();

	// Get function entry points
	vector<BPatch_point *> *points;
	try {
		points = getEntryPoints(appImage, function.c_str(), multiple, loc);
	} catch (NoEntryPoint &e) {
		throw PuppeteerError(e.what());
	}

	BPatch_callWhen when;
	if (loc == puppeteerEventLoc::puppeteerEventLocBefore)
		when = BPatch_callBefore;
	else if (loc == puppeteerEventLoc::puppeteerEventLocAfter)
		when = BPatch_callAfter;
	else
		throw InvalidArgument("Supplied loc not implementetd");

	// Get event hooks
	vector<BPatch_function *> startEvent, event, endEvent;
	try {
		getFunctions(appImage, "puppeteerStartEvent", false, startEvent);
		switch (eventId) {
		case puppeteerEventSimpleId:
			getFunctions(appImage, "puppeteerEventSimple", false, event);
			break;
		default:
			throw InvalidArgument("Supplied event not implemented");
			break;
		}
		getFunctions(appImage, "puppeteerEndEvent", false, endEvent);
	} catch (MultipleHooks &e) {
		throw PuppeteerError("More than one event functions found. Something is wrong.");
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

}

void puppeteerCtx::impl::addAction(const int delay, const function<void()> a)
{
	actions.insert(pair<int, function<void()>>(delay, a));
}

void puppeteerCtx::impl::eventListener(PuppetCtx &puppet)
{
	shmEventCtx_t *shmEvent = puppet.shmCtx.shm;

	while (true) {
		pthread_mutex_lock((pthread_mutex_t *)&shmEvent->event_m);

		// Wait until there is a new event
		while (shmEvent->eventBufferStart == shmEvent->eventBufferEnd)
			pthread_cond_wait((pthread_cond_t *)&shmEvent->event_c, (pthread_mutex_t *)&shmEvent->event_m);

		// Process all new events
		while (shmEvent->eventBufferStart != shmEvent->eventBufferEnd) {
			// Get event
			puppeteerEvent_t *event = &shmEvent->eventBuffer[shmEvent->eventBufferStart];

			// Store event
			puppet.eventsList.push_back(*event);

			//Next event
			shmEvent->eventBufferStart = (shmEvent->eventBufferStart + 1 ) % shmEvent->eventBufferSize;
		}

		pthread_mutex_unlock((pthread_mutex_t *)&shmEvent->event_m);

	}
}

void puppeteerCtx::impl::launchActions()
{
	for (auto &a : actions) {
		chrono::seconds next_action(a.first);
		this_thread::sleep_for(next_action);
		a.second();
	}
}

void puppeteerCtx::impl::waitTestEnd(const int secs)
{
	bool all_terminated = false;
	while (!all_terminated) {
		all_terminated = true;
		for (auto &p : puppets) {
			BPatch_process *appProc = dynamic_cast<BPatch_process *>(p.handle);
			if (!appProc->isTerminated()) {
				// Just in case the puppet has automatically stopped (it should not happen ever)
				appProc->continueExecution();
				all_terminated = false;
				break;
			}
		}

		if (all_terminated) {
			break;
		} else {
			bpatch.waitForStatusChange();
		}
	}
}

void puppeteerCtx::impl::endTest(const int secs, const bool force)
{
	chrono::seconds timeToEnd {secs};
	this_thread::sleep_for(timeToEnd);

	for (auto &p : puppets) {
		BPatch_process *appProc = dynamic_cast<BPatch_process *>(p.handle);

		if (force) {
			kill(appProc->getPid(), SIGKILL);
		} else {
			appProc->terminateExecution();
		}
	}
}

vector<puppeteerEvent_t> puppeteerCtx::impl::getEvents(const string puppetName)
{
	vector<PuppetCtx>::iterator it = findPuppet(puppetName);
	PuppetCtx p = *it;

	return p.eventsList;
}


void puppeteerCtx::impl::startTest(const int secs, const bool endTest, const bool forceEnd)
{
	// Launch event listener threads
	vector<thread> eventListenerThreads;
	for (auto &p : puppets) {
		if (p.shmCtx.initialized) {
			eventListenerThreads.emplace(eventListenerThreads.end(), thread {&puppeteerCtx::impl::eventListener, this, ref(p)});
			eventListenerThreads.back().detach();
		}

		BPatch_process *appProc = dynamic_cast<BPatch_process *>(p.handle);
		appProc->continueExecution();
	}

	// Launch actions and wait until all actions are performed.
	thread launchActionsThread {&puppeteerCtx::impl::launchActions, this};
	
	waitTestEnd(secs);
	if (endTest) {
		// Launch endTest thread.
		thread endTestThread {&puppeteerCtx::impl::endTest, this, secs, forceEnd};
		endTestThread.join();
	}

	launchActionsThread.join();
}
/**/


/* Function used as Dyninst callback */
void
detachThread(   BPatch_thread *parent,
                BPatch_thread *child    )
{
	BPatch_process *childProc = child->getProcess();
	childProc->stopExecution();
	childProc->detach(1);
}
/**/


/* Public interface */
puppeteerCtx::puppeteerCtx() : pimpl { new impl} {
	setenv("DYNINSTAPI_RT_LIB", dyinst_rt_lib, 0);
	setenv("PLATFORM", dyninst_platform, 0);

	/* Optimizations */
	// Turn on or off trampoline recursion.
	// By default, any snippets invoked while another snippet is active will not be executed.
	bpatch.setTrampRecursive(true);
	// Turn on or off floating point saves.
	bpatch.setSaveFPR(false);
	// We don't want to instrument childs
	// This reduces the time penalty introduced when puppeets create childs and
	// Dyninst analyzes them.
	bpatch.registerPostForkCallback(detachThread);
	/**/
}

puppeteerCtx::~puppeteerCtx()
{


}

void
puppeteerCtx::addPuppet(    const string puppetName,
                            const string puppetCmd  )
{
	pimpl->addPuppet(puppetName, puppetCmd);
}

void 
puppeteerCtx::initPuppets()
{
	pimpl->initPuppets();
}


void
puppeteerCtx::addEvent(  const string puppetName,
                         const string function,
                         const bool multiple,
                         const puppeteerEventLoc loc,
                         const puppeteerEvent_e eventId,
                         const char data[MAX_EVENT_DATA]   )
{
	pimpl->addEvent(puppetName, function, multiple, loc, eventId, data);
}

void
puppeteerCtx::addAction(const int delay, const function<void()> a)
{
	pimpl->addAction(delay, a);
}

void
puppeteerCtx::startTest(const int secs, const bool endTest, const bool forceEnd)
{
	pimpl->startTest(secs, endTest, forceEnd);
}

void
puppeteerCtx::endTest(const int secs, const bool force)
{
	pimpl->endTest(secs, force);
}

vector<puppeteerEvent_t> 
puppeteerCtx::getEvents(const string puppetName)
{
	return pimpl->getEvents(puppetName);
}

/**/