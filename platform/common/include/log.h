#ifndef INC_COMMON_LOG_H
#define INC_COMMON_LOG_H

#include <stdbool.h>
#include <syslog.h>
#include <errno.h>
#include "constants.h"

#define _FILE strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__
#define INFO_MSG(...) do {if (DEBUG) info_msg(__VA_ARGS__);} while(0);

//Functions
void init_log(const char *data_path);
void end_log();
void err_message(const bool useErr, const char* file, int line, const char *format, ...);
void info_message(const char* file, int line, const char *format, ...);
//end functions

//wrappers
#define err_msg(B, M, ...) err_message(B, _FILE, __LINE__, M, ##__VA_ARGS__)
#define info_msg(M, ...) info_message(_FILE, __LINE__, M, ##__VA_ARGS__)
//FIXME Remove when log_message is changed everywhere
#define log_message(P, M, B) \
do{ \
  if (P == LOG_ERR || P == LOG_WARNING) \
    err_msg(B, M); \
  else \
    info_msg(M); \
}while(0)

//end wrappers

#endif