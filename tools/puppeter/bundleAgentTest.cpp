#include "include/puppetMasterLib.h"

int main(int argc, char const *argv[])
{
	
	puppeteerCtx testEnv;
	testEnv.addPuppet(string("receiver"), string("/home/xeri/projects/adtn/root/bin/receiver -f -c /home/xeri/projects/adtn/test/adtn.ini"));
	testEnv.initPuppets();
	testEnv.addEvent(string("receiver"),  string("main"),  puppeteerEventLocBefore, puppeteerEventSimpleId, "1");
	testEnv.startTest(10);
	testEnv.waitTestEnd();
	testEnv.printEvents();
	

	return 0;
}