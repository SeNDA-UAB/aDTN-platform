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

#ifndef INC_COMMON_LOG_H
#define INC_COMMON_LOG_H

#include <stdbool.h>
#include <syslog.h>
#include <errno.h>
#include "constants.h"

#define _FILE strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__

//Functions
void init_log(const char *data_path);
void end_log();
void log_err_message(const int debug_level, const bool useErr, const char *file, int line, const char *format, ...);
void set_debug_lvl(const int lvl);
void set_log_file(const bool logf);
void log_info_message(const int debug_level, const char *file, int line, const char *format, ...);
//end functions

//wrappers
#define log_err(DL, B, M, ...) log_err_message(DL, B, _FILE, __LINE__, M, ##__VA_ARGS__)
#define log_info(DL, M, ...) log_info_message(DL, _FILE, __LINE__, M, ##__VA_ARGS__)

#define LOG_MSG(LVL, B, M, ...)                                         \
	do                                                                  \
	{                                                                   \
		if (LVL == LOG__ERROR || LVL == LOG__WARNING)               \
			log_err(LVL, B, M, ##__VA_ARGS__);         	            \
		else                                                        \
			log_info(LVL, M, ##__VA_ARGS__);		                \
	} while (0)

//end wrappers

#endif