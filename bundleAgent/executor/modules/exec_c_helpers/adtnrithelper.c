#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common/include/rit.h"

int set_rit_path(const char *rit_path)
{
	return rit_changePath(rit_path);
}

char *get(char *path)
{
	if (path == NULL)
		return NULL;

	return rit_getValue(path);
}

int set(char *path, char *value)
{
	if (path == NULL || *path == '\0' || value == NULL || *value == '\0')
		return 1;

	return rit_set(path, value);
}

int rm(char *path)
{
	if (path == NULL)
		return 1;

	return rit_unset(path);
}