#ifndef PUPPETEER_PUPPETMASTERLIB_H_
#define PUPPETEER_PUPPETMASTERLIB_H_

#include <string>
#include <thread>
#include <vector>

#include "puppeteerCommon.h"

using namespace std;

/* Exceptions */
class PuppetNotFound: public logic_error
{
public:
	PuppetNotFound(): logic_error("Puppet not found in puppeteer context.") {};
	PuppetNotFound(string errMsg): logic_error(errMsg) {};
};

class FunctionNotFound: public logic_error
{
public:
	FunctionNotFound(): logic_error("Function not found in puppet address space.") {};
	FunctionNotFound(string errMsg): logic_error(errMsg) {};
};

class MultipleHooks: public logic_error
{
public:
	MultipleHooks(): logic_error("More than one hook point") {};
	MultipleHooks(string errMsg): logic_error(errMsg) {};
};

class InvalidArgument: public invalid_argument
{
public:
	InvalidArgument(): invalid_argument("More than one function found in puppet address space.") {};
	InvalidArgument(string errMsg): invalid_argument(errMsg) {};
};

class PuppeteerError: public logic_error
{
public:
	PuppeteerError(): logic_error("Internal error.") {};
	PuppeteerError(string errMsg): logic_error(errMsg) {};
}; 
/**/

constexpr char dyninst_platform[] {"x86_64-unknown-linux2.4"};
constexpr char dyinst_rt_lib[] {"/usr/local/lib64/libdyninstAPI_RT.so"};
constexpr char puppet_lib_path[] {"/home/xeri/projects/adtn/repo/aDTN-platform/build/tools/puppeter/libpuppetLib.so"};

enum class puppeteerEventLoc
{
    puppeteerEventLocBefore,
    puppeteerEventLocAfter,
};

class puppeteerCtx
{
public:
	puppeteerCtx();
	~puppeteerCtx();
	void addPuppet(const string puppetName, const string puppetCmd);
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
private:
	// http://herbsutter.com/gotw/_100/
	class impl;
	unique_ptr<impl> pimpl;
};


#endif