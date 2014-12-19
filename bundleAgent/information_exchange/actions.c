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

int update_nbs_info(const char *nb_id, const char *nb_ip, const int nb_port, struct nbs_list *nbs)
{
	time_t recv_time = time(NULL);
	struct nb_info *nb = get_nb(nb_id, nbs);

	if (nb == NULL) {
		nb = init_nb(nb_id, nb_ip, nb_port, recv_time);
		add_nb(nb, nbs);
	} else {
		READ_LOCK(&nb->rwlock) ;
		nb->last_seen = recv_time;
		UNLOCK_LOCK(&nb->rwlock);
	}

	if (store_nbs_info_to_rit(nbs) != 0)
		LOG_MSG(LOG__ERROR, false, "Error storing updated nbs info into the RIT");

	return 0;
}