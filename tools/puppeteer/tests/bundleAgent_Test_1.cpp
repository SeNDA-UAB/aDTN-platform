#include "include/puppetMasterLib.hpp"
#include "include/puppetStats.hpp"

extern "C" {
#include "testUtils.c"
}

int main(int argc, char const *argv[])
{
	puppeteerCtx testPlat;

	printf("---- Starting receiving platform\n");
	launchPlatform("/home/xeri/projects/adtn/test/platforms/adtn2.ini");

	printf("---- Starting tested platform\n");	
	testPlat.addPuppet("information_exchange", 	"/home/xeri/projects/adtn/root/bin/information_exchange -f -c /home/xeri/projects/adtn/test/platforms/adtn.ini");
	testPlat.addPuppet("queueManager", 			"/home/xeri/projects/adtn/root/bin/queueManager -c /home/xeri/projects/adtn/test/platforms/adtn.ini");
	testPlat.addPuppet("processor", 			"/home/xeri/projects/adtn/root/bin/processor -c /home/xeri/projects/adtn/test/platforms/adtn.ini");
	testPlat.addPuppet("receiver", 				"/home/xeri/projects/adtn/root/bin/receiver -c /home/xeri/projects/adtn/test/platforms/adtn.ini");

	char *const executorArgs[] = {
		"executor",
		"-c", "/home/xeri/projects/adtn/test/platforms/adtn.ini",
		NULL
	};
	if (!fork()) {
		execve("/home/xeri/projects/adtn/root/bin/executor", executorArgs, NULL);
	}

	testPlat.initPuppets();
	testPlat.addEvent("receiver",       "load_and_process_bundle",  true, puppeteerEventLoc::puppeteerEventLocBefore,     puppeteerEventSimpleId,     "Bundle received");
	testPlat.addEvent("receiver",       "queue",                    true, puppeteerEventLoc::puppeteerEventLocAfter,    puppeteerEventSimpleId,     "Bundle enqueued");
	testPlat.addEvent("processor",      "process_bundle",           true, puppeteerEventLoc::puppeteerEventLocBefore,     puppeteerEventSimpleId,     "Bundle dequeued");
	testPlat.addEvent("processor",      "send_bundle",              true, puppeteerEventLoc::puppeteerEventLocAfter,    puppeteerEventSimpleId,     "Bundle sent");


	testPlat.addAction(10, [&] {
		int i;
		for (i = 0; i < 10; i++)
		{
			// Test 1.1
			//bundle_s *b = createBundle();
			// Test 1.2
			//bundle_s *b = createBundleWithForwardingCode();			
			// Test 1.3
			bundle_s *b = createBundleWithUniqueForwardingCode();
			char *b_name;
			sendBundle("/home/xeri/projects/adtn/root/var/lib/adtn/", b, &b_name);
			usleep(300000);
		}
		testPlat.endTest(10, true);
	});

	testPlat.startTest(0, false, false);

	// Get events
	vector<puppeteerEvent_t> receiverEvents = testPlat.getEvents("receiver");
	vector<puppeteerEvent_t> processorEvents = testPlat.getEvents("processor");

	// Prepare puppeterStats class
	puppeteerStats testPlatStats {receiverEvents};
	testPlatStats.mergeEvents(processorEvents);

	// Count events
	int bundles_recv, bundles_enqueued, bundles_dequeued, bundless_ready_to_send, bundles_sent;
	bundles_recv = testPlatStats.countEvents("Bundle received", strlen("Bundle received"));
	bundles_enqueued = testPlatStats.countEvents("Bundle enqueued", strlen("Bundle enqueued"));
	bundles_dequeued = testPlatStats.countEvents("Bundle dequeued", strlen("Bundle dequeued"));
	bundles_sent = testPlatStats.countEvents("Bundle sent", strlen("Bundle sent"));

	printf("Bundles received: %d\nBundles enqueued: %d\nBundles dequeued: %d\nBundles sent: %d\n",
	       bundles_recv, bundles_enqueued, bundles_dequeued, bundles_sent);

	puppeteerStats::dataStats recvSent  = testPlatStats.eventStats("Bundle received", strlen("Bundle received"),
	                                      "Bundle sent", strlen("Bundle sent"));
	puppeteerStats::dataStats recvEnqueued  = testPlatStats.eventStats("Bundle received", strlen("Bundle received"),
	        "Bundle enqueued", strlen("Bundle enqueued"));
	puppeteerStats::dataStats enqueuedDequeued  = testPlatStats.eventStats("Bundle enqueued", strlen("Bundle enqueued"),
	        "Bundle dequeued", strlen("Bundle dequeued"));
	puppeteerStats::dataStats dequeuedSent = testPlatStats.eventStats("Bundle dequeued", strlen("Bundle dequeued"),
	        "Bundle sent", strlen("Bundle sent"));

	printf("Recv-Sent; min: %lf , max: %lf , avg %lf \n", recvSent.min, recvSent.max, recvSent.avg);
	printf("Recv-Enqueued; min: %lf , max: %lf , avg %lf \n", recvEnqueued.min, recvEnqueued.max, recvEnqueued.avg);
	printf("Enqueued-Dequeued; min: %lf , max: %lf , avg %lf \n", enqueuedDequeued.min, enqueuedDequeued.max, enqueuedDequeued.avg);
	printf("Dequeued-Sent; min: %lf , max: %lf , avg %lf \n", dequeuedSent.min, dequeuedSent.max, dequeuedSent.avg);
	
	printf("\n\nExcel paste:\n");
	printf("%lf %lf %lf \n", recvSent.min, recvSent.max, recvSent.avg);
	printf("%lf %lf %lf \n", recvEnqueued.min, recvEnqueued.max, recvEnqueued.avg);
	printf("%lf %lf %lf \n", enqueuedDequeued.min, enqueuedDequeued.max, enqueuedDequeued.avg);
	printf("%lf %lf %lf \n", dequeuedSent.min, dequeuedSent.max, dequeuedSent.avg);	
	printf("\n\n\n");


	// Kill all forked processes
	sigset_t blocked_signals;
	sigemptyset(&blocked_signals);
	sigaddset(&blocked_signals, SIGINT);
	sigprocmask(SIG_BLOCK, &blocked_signals, NULL);
	kill(-getpgrp(), SIGINT);

	return 0;
}