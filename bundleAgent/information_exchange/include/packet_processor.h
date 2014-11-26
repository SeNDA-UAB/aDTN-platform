#ifndef INC_PACKETPROCESSOR_H
#define INC_PACKETPROCESSOR_H

struct reply_set {
	void *data;
	int lenght;
	char dest[16];
	int dest_port;
};

#endif