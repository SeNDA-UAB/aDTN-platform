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
	static int i = 0; // Dummy variable to force executor compile the routing code each time.

	bundle_s *b = bundle_new();

	// Primary block
	bundle_set_source(b, "local:1");
	bundle_set_destination(b, "xeri2:1");
	bundle_set_lifetime(b, 30);

	// Payload
	payload_block_s *p = bundle_new_payload_block();
	bundle_set_payload(p, (uint8_t *)"TEST_BUNDLE", strlen("TEST_BUNDLE"));

	// MMEB
	mmeb_s *mmeb = bundle_new_mmeb();
	char r_code[512];
	snprintf(r_code, sizeof(r_code), "int i=%d; if (nbs.has_next()){ add_hop(nbs.next());} return 0;", i++);
	bundle_add_mmeb_code(mmeb, ROUTING_CODE, 0x01, strlen(r_code), (uint8_t *)r_code);

	// Put all together
	bundle_add_ext_block(b, (ext_block_s *)p);
	bundle_add_ext_block(b, (ext_block_s *)mmeb);

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

	printf("---- Starting tested platform\n");
	puppeteerCtx testPlat;
	testPlat.addPuppet(string("information_exchange"), string("/home/xeri/projects/adtn/root/bin/information_exchange -f -c /home/xeri/projects/adtn/test/platforms/adtn.ini"));
	testPlat.addPuppet(string("queueManager"), string("/home/xeri/projects/adtn/root/bin/queueManager -c /home/xeri/projects/adtn/test/platforms/adtn.ini"));
	testPlat.addPuppet(string("processor"), string("/home/xeri/projects/adtn/root/bin/processor -c /home/xeri/projects/adtn/test/platforms/adtn.ini"));
	testPlat.addPuppet(string("receiver"), string("/home/xeri/projects/adtn/root/bin/receiver -c /home/xeri/projects/adtn/test/platforms/adtn.ini"));
	testPlat.addPuppet(string("executor"), string("/home/xeri/projects/adtn/root/bin/executor -c /home/xeri/projects/adtn/test/platforms/adtn.ini"));

	testPlat.initPuppets();
	testPlat.addEvent(string("executor"),  		string("execute_compile_cmd"),  puppeteerEventLocBefore, 	puppeteerEventSimpleId, 	"Compilation started");
	testPlat.addEvent(string("executor"),  		string("execute_compile_cmd"),  puppeteerEventLocAfter, 	puppeteerEventSimpleId, 	"Compilation ended");
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
	printf("Bundles received: %d\n", testPlat.countEvents(eventList, "Compilation started"));
	printf("Bundles enqueued: %d\n", testPlat.countEvents(eventList, "Compilation ended"));
	printf("\n");

	list< pair<const puppeteerEvent_t *, const puppeteerEvent_t *> > compilationTime;
	testPlat.pairEvents(eventList, "Compilation started", "Compilation ended",  compilationTime);
	list< pair<const puppeteerEvent_t *, const puppeteerEvent_t *> >::const_iterator it_compilationTime = compilationTime.cbegin();

	int i = 0;
	for (; it_compilationTime != compilationTime.cend(); ++it_compilationTime){
		printf("Compilation time %d:\t %lf\n", i, testPlat.getDiffTimestamp(*it_compilationTime->first, *it_compilationTime->second)/1.0e9);
		i++;
	}
	double min, max, avg;
	testPlat.getPairStats(compilationTime, max, min, avg);
	printf("Compilation time stats: min: %lfs, max: %lfs, avg: %lfs\n", min/1.0e9, max/1.0e9, avg/1.0e9);

	printf("Total time: %lf\n", testPlat.getDiffTimestamp(*eventList.begin(), *(--eventList.end()))/1.0e9 );


	return 0;
}