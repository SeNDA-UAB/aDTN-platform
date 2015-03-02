#include "include/puppetStats.hpp"

#include <cstring>

/* Private members */
puppeteerStats::puppeteerEventList::const_iterator
puppeteerStats::findNextEvent(  puppeteerStats::puppeteerEventList::const_iterator start,
                                puppeteerStats::puppeteerEventList::const_iterator end,
                                const char eventData[MAX_EVENT_DATA], const int eventData_l)
{
	auto next = start;
	for (; next != end; ++next) {
		auto e = *next;
		if (memcmp(e.second.data, eventData, eventData_l) == 0) {
			return next;
		}
	}
	return end;
}

void
puppeteerStats::pairEvents(    const char dataA[MAX_EVENT_DATA], const int dataA_l,
                               const char dataB[MAX_EVENT_DATA], const int dataB_l,
                               vector<pair<puppeteerEvent_t, puppeteerEvent_t>> &pairedEvents   )
{
	auto next = events.cbegin();
	for (   ;
	        next != events.cend();
	        ++next    ) {
		// Found dataA event
		if (memcmp(next->second.data, dataA, dataA_l) == 0) {
			// Find next dataB event
			puppeteerStats::puppeteerEventList::const_iterator b = findNextEvent(next, events.end(), dataB, dataB_l);
			if (b == events.end())
				throw UnpairedEvents("Pair not found");
			else
				pairedEvents.insert(pairedEvents.end(), pair<puppeteerEvent_t, puppeteerEvent_t> {next->second, b->second});
		}
	}

	// Check if there are missing A events
	for (; next != events.cend(); ++next) {
		if (memcmp(next->second.data, dataB, dataB_l) == 0) {
			throw UnpairedEvents("Not all elements have been paired");
		}
	}

}
double
puppeteerStats::diff_time(const struct timespec &start, const struct timespec &end)
{
	return (double)(end.tv_sec - start.tv_sec) * 1.0e9 + (double)(end.tv_nsec - start.tv_nsec);
}

/**/

/* Public members */
puppeteerStats::puppeteerStats(const vector<puppeteerEvent_t> &eventList)
{
	mergeEvents(eventList);
}

puppeteerStats::~puppeteerStats()
{
}

bool
puppeteerStats::timespecCmp::operator() (   const struct timespec &a,
        const struct timespec &b    )
{
	if (a.tv_sec != b.tv_sec) {
		return a.tv_sec < b.tv_sec;
	} else {
		return a.tv_nsec < b.tv_nsec;
	}
}

void
puppeteerStats::mergeEvents(const vector<puppeteerEvent_t> &eventList)
{
	for (auto &e : eventList) {
		events.insert(pair<struct timespec, puppeteerEvent_t> {e.timestamp, e});
	}
}

int puppeteerStats::countEvents(const char data[MAX_EVENT_DATA], const int data_l)
{
	int count = 0;

	for (auto &e : events) {
		if (memcmp(e.second.data, data, data_l) == 0)
			count++;
	}

	return count;
}

puppeteerStats::dataStats
puppeteerStats::eventStats(const char dataA[MAX_EVENT_DATA], const int dataA_l,
           const char dataB[MAX_EVENT_DATA], const int dataB_l)
{

	vector<pair<puppeteerEvent_t, puppeteerEvent_t>> pairedEvents;

	pairEvents(dataA, dataA_l, dataB, dataB_l, pairedEvents);

	puppeteerStats::dataStats stats = {.max = -1, .min = -1, .avg = 0};
	int count = 0;
	for (auto &p : pairedEvents) {
		double diff = diff_time(p.first.timestamp, p.second.timestamp)/1.0e9;

		// Initialization
		if (stats.min == -1)
			stats.min = diff;
		if (stats.max == -1)
			stats.max = diff;

		// Update
		if (diff < stats.min)
			stats.min = diff;
		else if (diff > stats.max)
			stats.max = diff;
		stats.avg += diff;
		count++;
	}

	stats.avg = stats.avg / count;

	return stats;
}
/**/