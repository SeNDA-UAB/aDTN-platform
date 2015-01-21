
#include <string>
#include "BPatch.h"
#include "puppeteerCommon.h"

enum puppeteerEventLoc_e {
	puppeteerEventLocBefore,
	puppeteerEventLocAfter,
};

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
	void getFunctions(BPatch_image *appImage, const string funcName, vector<BPatch_function *> &functions);

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
	int printEvents();
};