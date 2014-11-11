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
void log_info_message(const int debug_level, const char *file, int line, const char *format, ...);
//end functions

//wrappers
#define log_err(DL, B, M, ...) log_err_message(DL, B, _FILE, __LINE__, M, ##__VA_ARGS__)
#define log_info(DL, M, ...) log_info_message(DL, _FILE, __LINE__, M, ##__VA_ARGS__)

#define LOG_MSG(LVL, B, M, ...)                                         \
	do                                                                  \
	{                                                                   \
		if (DEBUG) {                                                    \
			if (LVL == LOG__ERROR || LVL == LOG__WARNING)               \
				log_err(LVL, B, M, ##__VA_ARGS__);         	            \
			else                                                        \
				log_info(LVL, M, ##__VA_ARGS__);		                \
		}                                                               \
	} while (0)

//end wrappers

#endif