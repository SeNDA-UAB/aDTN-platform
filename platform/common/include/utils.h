#ifndef INC_COMMON_UTILS_H
#define INC_COMMON_UTILS_H

#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>

char *generate_bundle_name();
int write_to(const char *path, const char *name, const uint8_t *content, const ssize_t content_l);
int get_file_size(FILE *fd);


#endif