/*
* Copyright (c) 2014 SeNDA
* 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* 
*     http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
* 
*/

#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/time.h>
#include <time.h>

#include "include/log.h"
#include "include/utils.h"

char *generate_bundle_name(const char *origin)
{
	struct timeval t_now = {0};
	char *name = NULL;

	if (gettimeofday(&t_now, NULL) < 0)
		goto end;

	asprintf(&name, "%ld-%ld-%s", t_now.tv_sec, t_now.tv_usec, origin);
end:

	return name;
}

int parse_bundle_name(const char *bundle_path, /*out*/b_name_s *b_name)
{
	int ret = 1;
	const char *p = NULL, *bundle_name = NULL;

	p = strrchr(bundle_path, '/');
	if (p) // It is a path
		bundle_name = p + 1;
	else
		bundle_name = bundle_path;

	if (sscanf(bundle_name, "%ld-%ld-%s",&b_name->sec, &b_name->usec, b_name->origin) != 3) {
		LOG_MSG(LOG__ERROR, false, "Error parsing timestamp of bundle %s", bundle_path);
		goto end;
	}

	ret = 0;
end:
	return ret;
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