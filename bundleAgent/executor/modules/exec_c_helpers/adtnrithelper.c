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