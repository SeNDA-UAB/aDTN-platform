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

int process_nd_query(struct nd_query *nd_query, int nd_query_l, const char *src_ip, /*out*/struct reply_set *reply)
{
	int ret = 1;
	reply->lenght = 0;

	if (nd_query_l != sizeof(struct nd_query)) {
		LOG_MSG(LOG__ERROR, false, "Malformed nd_query paquet received. Not the specified size.");
		goto end;
	}

	reply->lenght = build_nd_beacon((struct nd_beacon **)&reply->data);

	ret = 0;
end:
	return ret;
}

int process_nd_beacon(struct nd_beacon *nd_beacon, int nd_beacon_l, const char *src_ip)
{
	int ret = 1;
	char *rit_announceables_end = NULL;

	//Put NULL terminating char to avoid malformed rit_announcebles strings affect program.
	rit_announceables_end = (char *)nd_beacon + nd_beacon_l - 1;
	*rit_announceables_end = '\0';

	READ_LOCK(&world->nbs.rwlock);
	if (update_nbs_info(nd_beacon->platform_id, nd_beacon->platform_ip, nd_beacon->platform_port, &world->nbs) != 0)
		LOG_MSG(LOG__ERROR, false, "Error updating nbs info.");
	UNLOCK_LOCK(&world->nbs.rwlock);

	//Store announceables
	if (nd_beacon->rit_announceables != NULL && *nd_beacon->rit_announceables != '\0') {
		if (store_announceables(nd_beacon->rit_announceables) != 0) {
			LOG_MSG(LOG__ERROR, false, "Error storing announceable entries to the RIT");
			goto end;
		}
	}

	ret = 0;
end:
	return ret;
}