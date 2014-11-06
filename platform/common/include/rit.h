#ifndef INC_COMMON_RIT
#define INC_COMMON_RIT

#include "rit_paths.h"

//rit.h
#ifndef PREFIX
#define PREFIX "/usr/local"
#endif

#ifndef DEFAULT_CONF_FILE
#define DEFAULT_CONF_FILE PREFIX"/etc/adtn.ini"
#endif

#ifndef NBS_INFO
#define NBS_INFO "NBS"
#endif

typedef struct{
	int transaction;
}transaction_args;

/* VARIABLE FUNCTIONS */
#define rit_set(path, value, ...) rit_var_set(path, value, ##__VA_ARGS__) 
#define rit_unset(path, ...) rit_var_unset(path, ##__VA_ARGS__) 
#define rit_tag(path, name, value, ...) rit_var_tag(path, name, value, ##__VA_ARGS__) 
#define rit_untag(path, name, ...) rit_var_untag(path, name, ##__VA_ARGS__) 
#define rit_clear(path, ...) rit_var_clear(path, ##__VA_ARGS__) 
#define rit_delete(path, ...) rit_var_delete(path, ##__VA_ARGS__) 

int rit_var_set(const char* path, const char* value, ...);
int rit_var_unset(const char* path, ...);
int rit_var_tag(const char* path, const char* name, const char* value, ...);
int rit_var_untag(const char* path, const char* name, ...);
int rit_var_clear(const char* path, ...);
int rit_var_delete(const char *path, ...);

int rit_changePath(const char *newPath);
void rit_drop();
int rit_start();
int rit_commit();
int rit_rollback();

char* rit_getValue(const char *path);
char* rit_getTag(const char *path, const char *nom);

char* rit_getPathsByTag(const char *nom, const char* value);
char* rit_getTagNamesByPath(const char *path);
char* rit_getChilds(const char *path);

#endif
