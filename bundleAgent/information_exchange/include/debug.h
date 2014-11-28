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

#ifndef INC_DEBUG_H
#define INC_DEBUG_H

void log_nd_query(struct nd_query *nd_query, bool send);
void log_nd_beacon(struct nd_beacon *nd_beacon, bool send);
// void log_ix_req_list(struct ix_req_list *ix_req_list, bool send);
// void log_ix_list(struct ix_list *ix_list, bool send);
// void log_ix_req_data(struct ix_req_data *ix_req_data, bool send);
// void log_ix_data(struct ix_data *ix_data, bool send);
int log_nbs_table(struct nbs_list *nbs);

#endif