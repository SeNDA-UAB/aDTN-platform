#ifndef INC_COMMON_LOG_H
#define INC_COMMON_LOG_H

#include <stdbool.h>
#include <syslog.h>
#include <errno.h>
#include "constants.h"

#define _FILE strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__
#define INFO_MSG(...) do {if (DEBUG) info_msg(__VA_ARGS__);} while(0);
#define INFO_MSG1(...) do {if (DEBUG) info_msg1(__VA_ARGS__);} while(0);

//Functions
void init_log(const char *data_path);
void end_log();
void err_message(const bool useErr, const char *file, int line, const char *format, ...);
void log_err_message(const int debug_level, const bool useErr, const char *file, int line, const char *format, ...);
void info_message(const char *file, int line, const char *format, ...);
void set_debug_lvl(const int lvl);
void log_info_message(const int debug_level, const char *file, int line, const char *format, ...);
//end functions

//wrappers
#define log_err(DL, B, M, ...) log_err_message(DL, B, _FILE, __LINE__, M, ##__VA_ARGS__)
#define log_info(DL, M, ...) log_info_message(DL, _FILE, __LINE__, M, ##__VA_ARGS__)
#define err_msg(B, M, ...) err_message(B, _FILE, __LINE__, M, ##__VA_ARGS__)
#define info_msg(M, ...) info_message(_FILE, __LINE__, M, ##__VA_ARGS__)
#define info_msg1(DL, M, ...) log_message(DL, _FILE, __LINE__, M, ##__VA_ARGS__)
//FIXME Remove when log_message is changed everywhere
#define log_message(P, M, B) \
	do{ \
		if (P == LOG_ERR || P == LOG_WARNING) \
			err_msg(B, M); \
		else \
			info_msg(M); \
	}while(0)

//end wrappers
#define LOG_MSG(LVL, M, B)                                              \
	do                                                                  \
	{                                                                   \
		if (DEBUG) {                                                    \
			if (LVL == LOG__ERROR || LVL == LOG__WARNING)               \
				log_err(LVL, B, M);         		                    \
			else                                                        \
				log_info(LVL, M);			                            \
		}                                                               \
	} while (0)

#endif