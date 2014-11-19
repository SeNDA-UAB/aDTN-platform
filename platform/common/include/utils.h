#ifndef INC_COMMON_UTILS_H
#define INC_COMMON_UTILS_H

#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>

char *generate_bundle_name();
int write_to(const char *path, const char *name, const uint8_t *content, const ssize_t content_l);
int get_file_size(FILE *fd);
double diff_time(struct timespec *start, struct timespec *end);
void time_to_str(const struct timeval tv, char *time_str, int time_str_l);

#endif