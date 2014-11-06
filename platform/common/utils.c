#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "include/log.h"
#include "include/constants.h"
#include "include/utils.h"

#ifndef DEBUG
	#define DEBUG 0
#endif

char *generate_bundle_name()
{
	struct timeval t_now = {0};
	char *name = NULL;

	if (gettimeofday(&t_now, NULL) < 0)
		goto end;

	asprintf(&name, "%ld-%ld.bundle", t_now.tv_sec, t_now.tv_usec);
end:

	return name;
}

int write_to(const char *path, const char *name, const uint8_t *content, const ssize_t content_l)
{
	FILE *dest = NULL;
	int ret = 1;
	char *full_path = NULL;

	asprintf(&full_path, "%s/%s", path, name);

	if ((dest = fopen(full_path, "w")) == NULL) {
		err_msg(true, "Error opening %s", full_path);
		goto end;
	}

	if (fwrite(content, sizeof(*content), content_l, dest) != content_l) {
		err_msg(true, "Error writing to %s", full_path);
		goto end;
	}

	if (fclose(dest) != 0) {
		err_msg(true, "Error closing %s", full_path);
		goto end;
	}

	ret = 0;
end:
	if (full_path)
		free(full_path);

	return ret;
}