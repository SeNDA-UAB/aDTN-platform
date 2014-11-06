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