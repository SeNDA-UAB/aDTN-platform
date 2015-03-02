#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

#include "bundle.h"
#include "utils.h"
#include "executor.h"

double diff_time(struct timespec *start, struct timespec *end)
{
	return (double)(end->tv_sec - start->tv_sec) * 1.0e9 + (double)(end->tv_nsec - start->tv_nsec);
}

/********** aDTN commands **********/

int launchPlatform(char *const conf_file)
{
	// Launch information_exchange
	char *const informationExchangeArgs[] = {
		"information_exchange",
		"-f",
		"-c", conf_file,
		NULL
	};
	if (!fork()) {
		execve("/home/xeri/projects/adtn/root/bin/information_exchange", informationExchangeArgs, NULL);
	}

	// Launch queueManager
	char *const queueManagerArgs[] = {
		"queueManager",
		"-c", conf_file,
		NULL
	};
	if (!fork()) {
		execve("/home/xeri/projects/adtn/root/bin/queueManager", queueManagerArgs, NULL);
	}

	// Launch processor
	char *const processorArgs[] = {
		"processor",
		"-c", conf_file,
		NULL
	};
	if (!fork()) {
		execve("/home/xeri/projects/adtn/root/bin/processor", processorArgs, NULL);
	}

	// Launch receiver
	char *const receiverArgs[] = {
		"receiver",
		"-c", conf_file,
		NULL
	};
	if (!fork()) {
		execve("/home/xeri/projects/adtn/root/bin/receiver", receiverArgs, NULL);
	}

	// Launch executor
	char *const executorArgs[] = {
		"executor",
		"-c", conf_file,
		NULL
	};
	if (!fork()) {
		execve("/home/xeri/projects/adtn/root/bin/executor", executorArgs, NULL);
	}

	return 0;
}

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

bundle_s *createBundleWithUniqueForwardingCode()
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

bundle_s *createBundleWithForwardingCode()
{
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
	snprintf(r_code, sizeof(r_code), "if (nbs.has_next()){ add_hop(nbs.next());} return 0;");
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