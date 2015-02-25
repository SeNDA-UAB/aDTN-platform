#ifndef PUPPETEER_PUPPETSTATS_H_
#define PUPPETEER_PUPPETSTATS_H_

#include <map>
#include <vector>

extern "C" {
#include "puppeteerCommon.h"
}

using namespace std;

class puppeteerStats
{
public:
	/* Exceptions */
	class UnpairedEvents: public logic_error
	{
	public:
		UnpairedEvents(): logic_error("Can't pair all events.") {};
		UnpairedEvents(string errMsg): logic_error {errMsg} {};
	};
	/**/

	struct timespecCmp
	{
		bool operator() (const struct timespec &a, const struct timespec &b);
	};
	typedef multimap<struct timespec, puppeteerEvent_t, timespecCmp> puppeteerEventList;
	struct dataStats {
		double max;
		double min;
		double avg;
	};

private:
	puppeteerEventList events;
	puppeteerStats::puppeteerEventList::const_iterator
	findNextEvent(  puppeteerStats::puppeteerEventList::const_iterator start,
	                puppeteerStats::puppeteerEventList::const_iterator end,
	                const char eventData[MAX_EVENT_DATA], const int eventData_l);
	void
	pairEvents(    const char dataA[MAX_EVENT_DATA], const int dataA_l,
	               const char dataB[MAX_EVENT_DATA], const int dataB_l,
	               vector<pair<puppeteerEvent_t, puppeteerEvent_t>> &pairedEvents   );
	double diff_time(const struct timespec &start, const struct timespec &end);
public:
	puppeteerStats(const vector<puppeteerEvent_t> &);
	~puppeteerStats();

	void mergeEvents(const vector<puppeteerEvent_t> &);
	int countEvents(const char data[MAX_EVENT_DATA], const int data_l);
	dataStats eventStats(const char dataA[MAX_EVENT_DATA], const int dataA_l,
	                     const char dataB[MAX_EVENT_DATA], const int dataB_l);

	void getMergedEvents(puppeteerEventList &);
};

#endif