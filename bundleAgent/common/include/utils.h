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

#ifndef INC_COMMON_UTILS_H
#define INC_COMMON_UTILS_H

#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>

char *generate_bundle_name(const char *origin);
int write_to(const char *path, const char *name, const uint8_t *content, const ssize_t content_l);
int get_file_size(FILE *fd);
double diff_time(struct timespec *start, struct timespec *end);
void time_to_str(const struct timeval tv, char *time_str, int time_str_l);

#endif