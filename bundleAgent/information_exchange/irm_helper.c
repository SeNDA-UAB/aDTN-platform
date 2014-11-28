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

#define SNPRINTF(...)                                           \
	do{                                                         \
		int r = snprintf(__VA_ARGS__);                          \
		if (r >= MAX_RIT_PATH){                                 \
			LOG_MSG(LOG__ERROR, false, "nb_path too long");     \
			continue;                                           \
		} else if (r < 0){                                      \
			LOG_MSG(LOG__ERROR, true, "snprintf()");            \
			continue;                                           \
		}                                                       \
	} while(0);


//branch1=literal1|branch2=literal2|branch3=literal3
int generate_announceables_string(char **announceables)
{
	int ret = 0, off = 0;
	char    *announceables_paths = NULL,
	         *next_announceable = NULL,
	          *next_announceable_saveptr = NULL;

	announceables_paths = rit_getPathsByTag("announceable", "1");
	if (announceables_paths == NULL) {
		goto end;
	}

	*announceables = (char *)malloc(MAX_ANNOUNCEABLES_LEN * sizeof(char));

	next_announceable = strtok_r(announceables_paths, ";", &next_announceable_saveptr);
	while (next_announceable != NULL) {
		char *value = rit_getValue(next_announceable);
		if (value == NULL) {
			goto next;
		} else if (*value == '\0') {
			free(value);
			goto next;
		}

		off += snprintf(*announceables + off, MAX_ANNOUNCEABLES_LEN - off, "%s=%s|", next_announceable, value);
		free(value);

next:
		next_announceable = strtok_r(NULL, ";", &next_announceable_saveptr);
	}

	ret = off;

end:
	if (ret == 0)
		free(*announceables);

	return ret;
}

int store_announceables(char *rit_announceables)
{
	char    out_branch[MAX_RIT_PATH],
	        *next_announceable = NULL,
	         *next_announceable_saveptr = NULL;
	int off = 0, ret = 1;

	off = snprintf(out_branch, MAX_RIT_PATH, "%s", ANNOUNCED_INFO);
	next_announceable = strtok_r(rit_announceables, "|", &next_announceable_saveptr);
	while (next_announceable != NULL) {
		char *s = strchr(next_announceable, '=');
		if (s == NULL) {
			LOG_MSG(LOG__ERROR, false, "Invalid announceable, it does not has a value");
			goto next;
		}
		*s = '\0'; // Set termination char
		snprintf(out_branch + off, MAX_RIT_PATH - off, "%s", next_announceable);
		LOG_MSG(LOG__INFO, false, "Storing received announceable %s=%s", out_branch, s + 1);

		if (rit_set(out_branch, s + 1) != 0)
			LOG_MSG(LOG__ERROR, false, "rit_set()");
next:
		next_announceable = strtok_r(NULL, "|", &next_announceable_saveptr);
	}
	ret = 0;

	return ret;
}


int reset_nbs_info_from_rit()
{
	return rit_delete(NBS_BRANCH);
}

int store_nbs_info_to_rit(struct nbs_list *nbs)
{
	int ret = 1, off = 0;
	struct nb_info *elt = NULL, *tmp = NULL;
	char nb_path[MAX_RIT_PATH];

	rit_start();

	// Unset nbs
	if (rit_delete(NBS_BRANCH, 1) != 0)
		LOG_MSG(LOG__ERROR, false, "Error removing old nbs from the RIT");

	off = snprintf(nb_path, MAX_RIT_PATH, "%s/", NBS_INFO);
	if (off >= MAX_RIT_PATH) {
		LOG_MSG(LOG__ERROR, false, "NBS_INFO path too long");
		goto end;
	} else if (off < 0) {
		LOG_MSG(LOG__ERROR, true, "snprintf()");
		goto end;
	}

	// Update nbs
	HASH_ITER(hh, nbs->list, elt, tmp) {
		// Store IP
		SNPRINTF(nb_path + off, MAX_RIT_PATH - off, "%s/ip", elt->id);
		if (rit_set(nb_path, elt->ip, 1) != 0) {
			LOG_MSG(LOG__ERROR, false, "Error setting nb %s info into RIT", elt->id);
			continue;
		}
		// Store port
		SNPRINTF(nb_path + off, MAX_RIT_PATH - off, "%s/port", elt->id);
		char port_string[6];
		snprintf(port_string, sizeof(port_string), "%d", elt->port);
		if (rit_set(nb_path, port_string, 1) != 0) {
			LOG_MSG(LOG__ERROR, false, "Error setting nb %s info into RIT", elt->id);
			continue;
		}

	}

	ret = 0;
end:

	rit_commit();

	return ret;
}
