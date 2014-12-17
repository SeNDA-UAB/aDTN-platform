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

#ifndef INC_PACKET_H
#define INC_PACKET_H

#include "common/include/constants.h"

enum PAYLOADS {
	ND_BEACON = 0,
	ND_QUERY,
};

struct nd_beacon {
	unsigned char id;
	char platform_id[MAX_PLATFORM_ID_LEN];
	char platform_ip[STRING_IP_LEN];
	int platform_port;
	char rit_announceables[];
};

struct nd_query {
	unsigned char id;
	char platform_id[MAX_PLATFORM_ID_LEN];
};

#endif