#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "include/log.h"

static int initiated = 0;
static FILE *error_log_f;
static FILE *info_log_f;
static int debug_lvl = 4;

/*
Debug levels for log.

LOG__ERROR 0        Error conditions
LOG__WARNING 1      Warning conditions
LOG__INFO 2         Informational
LOG__DEBUG 3        Debug-level messages

*/

void init_log(const char *data_path)
{
	char *error_log_path = NULL;
	char *info_log_path = NULL; //Debug info is also written here

	if (NOLOG)
		return;

	errno = 0;
	int error_log_l = strlen(data_path) + strlen(ERROR_LOG_FILE) + 2;
	error_log_path = (char *)malloc(error_log_l);
	snprintf(error_log_path, error_log_l, "%s/%s", data_path, ERROR_LOG_FILE);
	error_log_f = fopen(error_log_path, "a");
	if (error_log_f == NULL)
		LOG_MSG(LOG__ERROR, true, "Error opening error logfile %s", error_log_path);

	int info_log_l = strlen(data_path) + strlen(INFO_LOG_FILE) + 2;
	info_log_path = (char *)malloc(info_log_l);
	snprintf(info_log_path, info_log_l, "%s/%s", data_path, INFO_LOG_FILE);
	info_log_f = fopen(info_log_path, "a");
	if (info_log_f == NULL)
		LOG_MSG(LOG__ERROR, false, "Error opening info logfile %s", info_log_path);

	free(error_log_path);
	free(info_log_path);

}

/* Based in Michael Kerrisk code from tlpi-dist package*/

static char *ename[] = {
	/*   0 */ "",
	/*   1 */ "EPERM", "ENOENT", "ESRCH", "EINTR", "EIO", "ENXIO",
	/*   7 */ "E2BIG", "ENOEXEC", "EBADF", "ECHILD",
	/*  11 */ "EAGAIN/EWOULDBLOCK", "ENOMEM", "EACCES", "EFAULT",
	/*  15 */ "ENOTBLK", "EBUSY", "EEXIST", "EXDEV", "ENODEV",
	/*  20 */ "ENOTDIR", "EISDIR", "EINVAL", "ENFILE", "EMFILE",
	/*  25 */ "ENOTTY", "ETXTBSY", "EFBIG", "ENOSPC", "ESPIPE",
	/*  30 */ "EROFS", "EMLINK", "EPIPE", "EDOM", "ERANGE",
	/*  35 */ "EDEADLK/EDEADLOCK", "ENAMETOOLONG", "ENOLCK", "ENOSYS",
	/*  39 */ "ENOTEMPTY", "ELOOP", "", "ENOMSG", "EIDRM", "ECHRNG",
	/*  45 */ "EL2NSYNC", "EL3HLT", "EL3RST", "ELNRNG", "EUNATCH",
	/*  50 */ "ENOCSI", "EL2HLT", "EBADE", "EBADR", "EXFULL", "ENOANO",
	/*  56 */ "EBADRQC", "EBADSLT", "", "EBFONT", "ENOSTR", "ENODATA",
	/*  62 */ "ETIME", "ENOSR", "ENONET", "ENOPKG", "EREMOTE",
	/*  67 */ "ENOLINK", "EADV", "ESRMNT", "ECOMM", "EPROTO",
	/*  72 */ "EMULTIHOP", "EDOTDOT", "EBADMSG", "EOVERFLOW",
	/*  76 */ "ENOTUNIQ", "EBADFD", "EREMCHG", "ELIBACC", "ELIBBAD",
	/*  81 */ "ELIBSCN", "ELIBMAX", "ELIBEXEC", "EILSEQ", "ERESTART",
	/*  86 */ "ESTRPIPE", "EUSERS", "ENOTSOCK", "EDESTADDRREQ",
	/*  90 */ "EMSGSIZE", "EPROTOTYPE", "ENOPROTOOPT",
	/*  93 */ "EPROTONOSUPPORT", "ESOCKTNOSUPPORT",
	/*  95 */ "EOPNOTSUPP/ENOTSUP", "EPFNOSUPPORT", "EAFNOSUPPORT",
	/*  98 */ "EADDRINUSE", "EADDRNOTAVAIL", "ENETDOWN", "ENETUNREACH",
	/* 102 */ "ENETRESET", "ECONNABORTED", "ECONNRESET", "ENOBUFS",
	/* 106 */ "EISCONN", "ENOTCONN", "ESHUTDOWN", "ETOOMANYREFS",
	/* 110 */ "ETIMEDOUT", "ECONNREFUSED", "EHOSTDOWN", "EHOSTUNREACH",
	/* 114 */ "EALREADY", "EINPROGRESS", "ESTALE", "EUCLEAN",
	/* 118 */ "ENOTNAM", "ENAVAIL", "EISNAM", "EREMOTEIO", "EDQUOT",
	/* 123 */ "ENOMEDIUM", "EMEDIUMTYPE", "ECANCELED", "ENOKEY",
	/* 127 */ "EKEYEXPIRED", "EKEYREVOKED", "EKEYREJECTED",
	/* 130 */ "EOWNERDEAD", "ENOTRECOVERABLE", "ERFKILL", "EHWPOISON"
};

char *get_formatted_time()
{

	time_t rawtime;
	struct tm *timeinfo;

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	static char retval[20];
	strftime(retval, sizeof(retval), "%Y-%m-%d %H:%M:%S", timeinfo);

	return retval;
}

static void generate_errmsg(bool useErr, int err,
                            char buf[MAX_LOG_MSG], const char *format, const char *file, int line, va_list ap)
{
	char userMsg[MAX_LOG_MSG] = {0}, errText[MAX_LOG_MSG] = {0};

	vsnprintf(userMsg, MAX_LOG_MSG, format, ap);

	if (useErr) {
		snprintf(errText, MAX_LOG_MSG, "%s (%s)",
		         (err > 0 && err <= MAX_ENAME) ?
		         ename[err] : "?UNKNOWN?", strerror(err));
		snprintf(buf, MAX_LOG_MSG, "%s ERROR[%s:%d] %s %s\n", get_formatted_time(), file, line, errText, userMsg);
	} else {
		snprintf(buf, MAX_LOG_MSG, "%s ERROR[%s:%d] %s\n", get_formatted_time(), file, line, userMsg);
	}


}

static void generate_infomsg(char buf[MAX_LOG_MSG],
                             const char *format, const char *file, int line, va_list ap)
{
	char userMsg[MAX_LOG_MSG];
	vsnprintf(userMsg, MAX_LOG_MSG, format, ap);
	snprintf(buf, MAX_LOG_MSG, "%s INFO[%s:%d] %s\n", get_formatted_time(), file, line, userMsg);
}

static void generate_warnmsg(bool useErr, int err,
                             char buf[MAX_LOG_MSG], const char *format, const char *file, int line, va_list ap)
{
	char userMsg[MAX_LOG_MSG];
	vsnprintf(userMsg, MAX_LOG_MSG, format, ap);
	snprintf(buf, MAX_LOG_MSG, "%s WARNING[%s:%d] %s\n", get_formatted_time(), file, line, userMsg);
}

//Writes to info log
void log_info_message(const int debug_level, const char *file, int line, const char *format, ...)
{
	va_list argList;
	int savedErrno;
	char buf[MAX_LOG_MSG];

	savedErrno = errno;       /* In case we change it here */

	if (debug_level <= debug_lvl) {
		va_start(argList, format);
		generate_infomsg(buf, format, file, line, argList);
		va_end(argList);

		printf("%s", buf);
		fflush(stdout);
		if (info_log_f != NULL) {
			fwrite(buf, strlen(buf), 1, info_log_f);
			fflush(info_log_f);
		}
	}

	errno = savedErrno;
}

// Writes to error log and stderr
void log_err_message(const int debug_level, const bool useErr, const char *file, int line, const char *format, ...)
{
	va_list argList;
	int savedErrno;
	char buf[MAX_LOG_MSG];

	savedErrno = errno;       /* In case we change it here */

	if (debug_level <= debug_lvl) {
		va_start(argList, format);
		if (debug_level == LOG__ERROR)
			generate_errmsg(useErr, errno, buf, format, file, line, argList);
		else
			generate_warnmsg(useErr, errno, buf, format, file, line, argList);
		va_end(argList);

		fputs(buf, stderr);
		if (error_log_f != NULL) {
			fwrite(buf, strlen(buf), 1, error_log_f);
			fflush(error_log_f);
		}
	}

	errno = savedErrno;
}

/**/

void end_log()
{
	if (error_log_f != NULL)
		fclose(error_log_f);
	if (info_log_f != NULL)
		fclose(info_log_f);
}

void set_debug_lvl(const int lvl)
{
	if (lvl < 0)
		debug_lvl = 0;
	else if (lvl > 4)
		debug_lvl = 4;
	else
		debug_lvl = lvl;
}
