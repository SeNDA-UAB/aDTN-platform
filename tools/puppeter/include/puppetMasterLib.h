
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
	int pos;
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
	int addEvent(const string puppetName, const string function,
	             const puppeteerEventLoc_e loc,
	             const puppeteerEvent_e eventId,
	             const char data[MAX_EVENT_DATA]); // data must be NULL terminated
	int startTest(const int secs);
	int waitTestEnd();

	/**/
	int printEvents();
	int getEvents(list<puppeteerEvent_t> &eventList);
	double getDiffTimestamp(const puppeteerEvent_t &a, const puppeteerEvent_t &b);
	int countEvents(list<puppeteerEvent_t> &eventList, const char data[MAX_EVENT_DATA]);
	const puppeteerEvent_t *findNextEvent(list<puppeteerEvent_t>::const_iterator &start, const list<puppeteerEvent_t>::const_iterator &end, const char eventData[MAX_EVENT_DATA]);
	int pairEvents( list<puppeteerEvent_t> &eventList,
	                const char a[MAX_EVENT_DATA],
	                const char b[MAX_EVENT_DATA],
	                list< pair<const puppeteerEvent_t *, const puppeteerEvent_t *> > &pairedList );
	int getPairStats(list< pair<const puppeteerEvent_t *, const puppeteerEvent_t *> > &pairedList, double &max, double &min, double &avg);
	/**/
};