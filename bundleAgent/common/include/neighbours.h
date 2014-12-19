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

#ifndef INC_COMMON_NEIGHBOURS_H
#define INC_COMMON_NEIGHBOURS_H

#include "constants.h"

int get_nbs(char **nbs_list); // Returns num of nbs
int get_nb_ip(const char *nb_id, /*out*/char **ip);
int get_nb_port(const char *nb_id, /*out*/int *port);
int get_nb_subscriptions(const char *nb_id, /*out*/ char **subscribed);

#endif
