#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/time.h>
#include <time.h>

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
		LOG_MSG(LOG__ERROR, true, "Error opening %s", full_path);
		goto end;
	}

	if (fwrite(content, sizeof(*content), content_l, dest) != content_l) {
		LOG_MSG(LOG__ERROR, true, "Error writing to %s", full_path);
		goto end;
	}

	if (fclose(dest) != 0) {
		LOG_MSG(LOG__ERROR, true, "Error closing %s", full_path);
		goto end;
	}

	ret = 0;
end:
	if (full_path)
		free(full_path);

	return ret;
}

int get_file_size(FILE *fd)
{
	int total_bytes;

	if (fd == NULL)
		return 0;
	fseek(fd, 0L, SEEK_END);
	total_bytes = (int)ftell(fd);
	fseek(fd, 0, SEEK_SET);

	return total_bytes;
}

/* time functions */
double diff_time(struct timespec *start, struct timespec *end)
{
	return ((double)(end->tv_sec - start->tv_sec) * 1.0e9 + (double)(end->tv_nsec - start->tv_nsec));
}

void time_to_str(const struct timeval tv, char *time_str, int time_str_l)
{
	int off = 0;
	struct tm *nowtm;
	nowtm = localtime(&tv.tv_sec);

	off = strftime(time_str, time_str_l, "%Y-%m-%d %H:%M:%S", nowtm);
	sprintf(time_str + off, ".%.6ld", tv.tv_usec);

}
/**/