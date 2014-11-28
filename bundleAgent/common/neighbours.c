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

#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <linux/netdevice.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "common.h"

int get_nbs(char **nbs_list_p)
{
	int num_nbs = 0, ret = 0, nbs_list_p_l = 0, i = 0;
	char *nbs = NULL, *next_nb = NULL;

	// It should return nb1;nb2;nb3;
	nbs = rit_getChilds(NBS_INFO);
	if (nbs == NULL) {
		goto end;
	} else if (*nbs == '\0') {
		free(nbs);
		goto end;
	}

	//Count nbs
	num_nbs++; // First nb
	for (   next_nb = strchr(nbs, ';');
	        next_nb != NULL;
	        next_nb = strchr(next_nb, ';')
	    ) {
		// Since we are here, we split the nbs
		*next_nb = '\0';
		next_nb++;

		num_nbs++;
	}

	nbs_list_p_l = num_nbs * MAX_PLATFORM_ID_LEN * sizeof(char);
	*nbs_list_p = (char *)calloc(1, nbs_list_p_l);

	//Copy nbs
	for (   next_nb = nbs;
	        *next_nb != '\0';
	        next_nb = strchr(next_nb, '\0') + 1, i++
	    ) {

		snprintf(*nbs_list_p + MAX_PLATFORM_ID_LEN * i, nbs_list_p_l - MAX_PLATFORM_ID_LEN * i, "%s", next_nb);
	}

	ret = num_nbs;
end:

	return ret;
}

int get_nb_ip(const char *nb_id, /*out*/char **ip)
{
	int ret = 1, r = 0;
	char ip_path[MAX_RIT_PATH];

	if (nb_id == NULL || ip == NULL)
		goto end;

	r = snprintf(ip_path, MAX_RIT_PATH, "%s/%s/ip", NBS_INFO, nb_id);
	if (r > MAX_RIT_PATH) {
		LOG_MSG(LOG__ERROR, false, "Rit path too long");
		goto end;
	} else if (r < 0) {
		LOG_MSG(LOG__ERROR, true, "snprintf()");
		goto end;
	}

	*ip = rit_getValue(ip_path);
	if (*ip != NULL && **ip != '\0')
		ret = 0;

end:

	return ret;
}

int get_nb_port(const char *nb_id, /*out*/int *port)
{
	int ret = 1, r = 0;
	char port_path[MAX_RIT_PATH], *string_port = NULL;

	if (nb_id == NULL || port == NULL)
		goto end;

	r = snprintf(port_path, MAX_RIT_PATH, "%s/%s/port", NBS_INFO, nb_id);
	if (r > MAX_RIT_PATH) {
		LOG_MSG(LOG__ERROR, false, "Rit path too long");
		goto end;
	} else if (r < 0) {
		LOG_MSG(LOG__ERROR, true, "snprintf()");
		goto end;
	}

	string_port = rit_getValue(port_path);
	if (string_port != NULL) {
		*port = strtol(string_port, NULL, 10);
		free(string_port);
		ret = 0;
	}

end:

	return ret;
}

int get_nb_subscriptions(const char *nb_id, /*out*/ char **subscribed)
{
	int ret = 1, r = 0;
	char subsc_path[MAX_RIT_PATH];

	if (nb_id == NULL || subscribed == NULL)
		goto end;

	r = snprintf(subsc_path, MAX_RIT_PATH, RIT_SUBSCRIBED_PATH, nb_id);
	if (r > MAX_RIT_PATH) {
		LOG_MSG(LOG__ERROR, false, "Rit path too long");
		goto end;
	} else if (r < 0) {
		LOG_MSG(LOG__ERROR, true, "snprintf()");
		goto end;
	}

	*subscribed = rit_getValue(subsc_path);

	if (*subscribed != NULL)
		ret = 0;

end:

	return ret;
}

