#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "include/puppetMasterLib.h"

extern "C" {
#include "bundle.h"
#include "utils.h"
#include "executor.h"
}

double diff_time(struct timespec *start, struct timespec *end)
{
	return (double)(end->tv_sec - start->tv_sec) * 1.0e9 + (double)(end->tv_nsec - start->tv_nsec);
}

/********** aDTN commands **********/

bundle_s *createBundle()
{
	bundle_s *b = bundle_new();

	// Primary block
	bundle_set_source(b, "local:1");
	bundle_set_destination(b, "xeri2:1");
	bundle_set_lifetime(b, 30);

	// Payload
	payload_block_s *p = bundle_new_payload_block();
	bundle_set_payload(p, (uint8_t *)"TEST_BUNDLE", strlen("TEST_BUNDLE"));

	// Put all together
	bundle_add_ext_block(b, (ext_block_s *)p);

	return b;
}

int sendBundle(const char *platformDataPath, bundle_s *b, /*out*/char **b_name)
{
	// Create bundle
	uint8_t *b_raw;
	int b_l = bundle_create_raw(b , &b_raw);

	// Write bundle to receiver incoming folder
	char input_path[PATH_MAX];
	snprintf(input_path, PATH_MAX, "%s/%s", platformDataPath, INPUT_PATH);

	*b_name = generate_bundle_name("test");
	write_to(input_path, *b_name, b_raw, b_l);

	return 0;
}

// TODO: Add it to queueManager
int storeBundleIntoQueue(const char *platformDataPath, bundle_s *b, /*out*/char **b_name)
{
	// Create bundle
	uint8_t *b_raw;
	int b_l = bundle_create_raw(b , &b_raw);

	// Write bundle to receiver incoming folder
	char input_path[PATH_MAX];
	snprintf(input_path, PATH_MAX, "%s/%s", platformDataPath, QUEUE_PATH);

	*b_name = generate_bundle_name("test");
	write_to(input_path, *b_name, b_raw, b_l);

	return 0;
}

int executeCode(const char *platformDataPath, const char *b_name)
{
	struct _petition p;
	union _response r;

	bzero(&p, sizeof(p));
	bzero(&r, sizeof(r));

	// Prepare executor comm.
	int test_s = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (test_s < 0) {
		perror("socket()");
		return 1;
	}
	struct sockaddr_un test_s_addr;
	bzero(&test_s_addr, sizeof(test_s_addr));
	test_s_addr.sun_family = AF_UNIX;
	srand(time(NULL));
	snprintf(test_s_addr.sun_path, sizeof(test_s_addr.sun_path), "/tmp/%d.sock", rand());
	unlink(test_s_addr.sun_path);
	if (bind(test_s, (struct sockaddr *)&test_s_addr, sizeof(struct sockaddr_un)) == -1) {
		perror("bind()");
		close(test_s);
		return 1;
	}

	// Prepare petition
	p.header.petition_type = EXE;
	p.header.code_type = ROUTING_CODE;
	strncpy(p.header.bundle_id, b_name, sizeof(p.header.bundle_id));

	// Send petition
	struct sockaddr_un exec_addr;
	bzero(&exec_addr, sizeof(exec_addr));
	exec_addr.sun_family = AF_UNIX;
	snprintf(exec_addr.sun_path, sizeof(exec_addr.sun_path), "/%s/%s", platformDataPath, "executor.sock");
	if (sendto(test_s, &p, sizeof(p), 0, (struct sockaddr *)&exec_addr, (socklen_t)sizeof(exec_addr)) < 0) {
		perror("sendto()");
		close(test_s);
		return 1;
	}

	// Get response
	int recv_l = recv(test_s, &r, sizeof(r), 0);
	if (recv_l < 0) {
		perror("recv()");
		close(test_s);
	}

	// Disconnect from executor
	close(test_s);

	return 0;
}

/********************/

int main(int argc, char const *argv[])
{

	// Prepare and start receiving platform
	// printf("---- Starting receiver platform\n");
	// puppeteerCtx recvPlat;
	// recvPlat.addPuppet(string("information_exchange"), string("/home/xeri/projects/adtn/root/bin/information_exchange -f -c /home/xeri/projects/adtn/test/platforms/adtn2.ini"));
	// recvPlat.addPuppet(string("queueManager"), string("/home/xeri/projects/adtn/root/bin/queueManager -c /home/xeri/projects/adtn/test/platforms/adtn2.ini"));
	// recvPlat.addPuppet(string("processor"), string("/home/xeri/projects/adtn/root/bin/processor -c /home/xeri/projects/adtn/test/platforms/adtn2.ini"));
	// recvPlat.addPuppet(string("receiver"), string("/home/xeri/projects/adtn/root/bin/receiver -c /home/xeri/projects/adtn/test/platforms/adtn2.ini"));
	// recvPlat.addPuppet(string("executor"), string("/home/xeri/projects/adtn/root/bin/executor -c /home/xeri/projects/adtn/test/platforms/adtn2.ini"));
	// recvPlat.initPuppets();
	// recvPlat.startTest(60);

	printf("---- Starting tested platform\n");
	puppeteerCtx testPlat;
	testPlat.addPuppet(string("information_exchange"), string("/home/xeri/projects/adtn/root/bin/information_exchange -f -c /home/xeri/projects/adtn/test/platforms/adtn.ini"));
	testPlat.addPuppet(string("queueManager"), string("/home/xeri/projects/adtn/root/bin/queueManager -c /home/xeri/projects/adtn/test/platforms/adtn.ini"));
	testPlat.addPuppet(string("processor"), string("/home/xeri/projects/adtn/root/bin/processor -c /home/xeri/projects/adtn/test/platforms/adtn.ini"));
	testPlat.addPuppet(string("receiver"), string("/home/xeri/projects/adtn/root/bin/receiver -c /home/xeri/projects/adtn/test/platforms/adtn.ini"));
	testPlat.addPuppet(string("executor"), string("/home/xeri/projects/adtn/root/bin/executor -c /home/xeri/projects/adtn/test/platforms/adtn.ini"));

	testPlat.initPuppets();
	testPlat.addEvent(string("receiver"),  		string("load_and_process_bundle"),  puppeteerEventLocBefore, 	puppeteerEventSimpleId, 	"Bundle received");
	testPlat.addEvent(string("receiver"),  		string("queue"),  					puppeteerEventLocAfter, 	puppeteerEventSimpleId, 	"Bundle enqueued");
	testPlat.addEvent(string("processor"),  	string("process_bundle"),  			puppeteerEventLocBefore, 	puppeteerEventSimpleId, 	"Bundle dequeued");
	testPlat.addEvent(string("processor"),  	string("send_bundle"),  			puppeteerEventLocAfter, 	puppeteerEventSimpleId, 	"Bundle sent");
	testPlat.startTest(60);

	if (!fork()) {

		// Wait until nb is discovered
		sleep(10);
		int i;
		for (i = 0; i < 10; i++) {
			bundle_s *b = createBundle();
			char *b_name;
			sendBundle("/home/xeri/projects/adtn/root/var/lib/adtn/", b, &b_name);
			sleep(2);
		}
		_exit(0);
	}

	testPlat.waitTestEnd();

	list<puppeteerEvent_t> eventList;
	testPlat.getEvents(eventList);


	printf("Number of events: %ld\n", eventList.size());
	printf("Bundles received: %d\n", testPlat.countEvents(eventList, "Bundle received"));
	printf("Bundles enqueued: %d\n", testPlat.countEvents(eventList, "Bundle enqueued"));
	printf("Bundles dequeued: %d\n", testPlat.countEvents(eventList, "Bundle dequeued"));
	printf("Bundles sent: %d\n", testPlat.countEvents(eventList, "Bundle sent"));
	printf("\n");

	list< pair<const puppeteerEvent_t *, const puppeteerEvent_t *> > recvQueued;
	testPlat.pairEvents(eventList, "Bundle received", "Bundle enqueued",  recvQueued);

	list< pair<const puppeteerEvent_t *, const puppeteerEvent_t *> > queuedDequeued;
	testPlat.pairEvents(eventList, "Bundle enqueued", "Bundle dequeued",  queuedDequeued);

	list< pair<const puppeteerEvent_t *, const puppeteerEvent_t *> > dequeuedSent;
	testPlat.pairEvents(eventList, "Bundle dequeued", "Bundle sent",  dequeuedSent);

	list< pair<const puppeteerEvent_t *, const puppeteerEvent_t *> > pairedList;
	testPlat.pairEvents(eventList, "Bundle received", "Bundle sent",  pairedList);

	int i = 1;
	list< pair<const puppeteerEvent_t *, const puppeteerEvent_t *> >::const_iterator it_recvQueued = recvQueued.cbegin();
	list< pair<const puppeteerEvent_t *, const puppeteerEvent_t *> >::const_iterator it_queuedDequeued = queuedDequeued.cbegin();
	list< pair<const puppeteerEvent_t *, const puppeteerEvent_t *> >::const_iterator it_dequeuedSent = dequeuedSent.cbegin();
	list< pair<const puppeteerEvent_t *, const puppeteerEvent_t *> >::const_iterator it_pairedList = pairedList.cbegin();

	for (; it_pairedList != pairedList.cend(); ++it_recvQueued, ++it_queuedDequeued, ++it_dequeuedSent, ++it_pairedList){

		printf("Time to send bundle %d:\t %lf\n", i, testPlat.getDiffTimestamp(*it_pairedList->first, *it_pairedList->second)/1.0e9);
		printf("Recveived-Enqueued %d:\t %lf\n", i, testPlat.getDiffTimestamp(*it_recvQueued->first, *it_recvQueued->second)/1.0e9);
		printf("Enqueued-Dequeued %d:\t %lf\n", i, testPlat.getDiffTimestamp(*it_queuedDequeued->first, *it_queuedDequeued->second)/1.0e9);
		printf("Dequeued-Sent %d:\t %lf\n", i, testPlat.getDiffTimestamp(*it_dequeuedSent->first, *it_dequeuedSent->second)/1.0e9);
		printf("\n");
		i++;
	}
	double min, max, avg;
	testPlat.getPairStats(pairedList, max, min, avg);
	printf("Received-Sent stats: min: %lfs, max: %lfs, avg: %lfs\n", min/1.0e9, max/1.0e9, avg/1.0e9);
	testPlat.getPairStats(recvQueued, max, min, avg);
	printf("Received-Enqueue stats: min: %lfs, max: %lfs, avg: %lfs\n", min/1.0e9, max/1.0e9, avg/1.0e9);
	testPlat.getPairStats(queuedDequeued, max, min, avg);
	printf("Enqueued-Dequeued stats: min: %lfs, max: %lfs, avg: %lfs\n", min/1.0e9, max/1.0e9, avg/1.0e9);
	testPlat.getPairStats(dequeuedSent, max, min, avg);
	printf("Dequeued-Sent stats: min: %lfs, max: %lfs, avg: %lfs\n", min/1.0e9, max/1.0e9, avg/1.0e9);

	printf("Total time: %lf\n", testPlat.getDiffTimestamp(*eventList.begin(), *(--eventList.end()))/1.0e9 );




	return 0;
}