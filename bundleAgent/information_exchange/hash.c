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

struct nb_info *init_nb(const char *id, const char *ip, const int port, long int first_seen)
{
	struct nb_info *nb;

	nb = (struct nb_info *)malloc(sizeof(struct nb_info));

	strncpy(nb->id, id, sizeof(nb->id));
	nb->id[sizeof(nb->id) - 1] = '\0'; // Force null byte finished
	strncpy(nb->ip, ip, sizeof(nb->ip));
	nb->ip[sizeof(nb->ip) - 1] = '\0'; // Force null byte finished
	nb->port = port;

	nb->first_seen = first_seen;
	nb->last_seen = first_seen;
	if (pthread_rwlock_init(&nb->rwlock, NULL) != 0)
		LOG_MSG(LOG__ERROR, true, "pthread_rwlock_init()");

	return nb;
}

struct nb_info *get_nb(const char *id, struct nbs_list *nbs)
{
	struct nb_info *nb = NULL;
	HASH_FIND_STR(nbs->list, id, nb);

	return nb;
}

int add_nb(struct nb_info *new_nb, struct nbs_list *nbs)
{
	HASH_ADD_STR(nbs->list, id, new_nb);
	nbs->num_nbs++;

	return 0;
}