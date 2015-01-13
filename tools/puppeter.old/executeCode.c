#include <linux/limits.h> // PATH_MAX
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>

#include "bundle.h"
#include "utils.h"
#include "executor.h"

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

// TODO: Add it to queueManager
int storeBundleIntoQueue(const char *platformDataPath, bundle_s *b, /*out*/char **b_name)
{
	// Create bundle
	uint8_t *b_raw;
	int b_l = bundle_create_raw(b , &b_raw);

	// Write bundle to receiver incoming folder
	char input_path[PATH_MAX];
	snprintf(input_path, PATH_MAX, "%s/%s", platformDataPath, QUEUE_PATH);

	*b_name = generate_bundle_name("local");
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

int main(int argc, char const *argv[])
{
	
		// Force 30 compilations
	bundle_s *b = createBundle();
	char *b_name;
	storeBundleIntoQueue("/home/xeri/projects/adtn/root/var/lib/adtn", b, &b_name);
	executeCode("/home/xeri/projects/adtn/root/var/lib/adtn", b_name);



	return 0;
}