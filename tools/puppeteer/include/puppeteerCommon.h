#ifndef PUPPETEER_COMMON_H_
#define PUPPETEER_COMMON_H_

#include <time.h> //struct timespec
#include <stdint.h> //uint8_t
#include <pthread.h> //pthread_*

#define MAX_EVENT_DATA 512 
#define MAX_EVENTS 64

/* event info*/
enum puppeteerEvent_e {
	puppeteerEventSimpleId
};

typedef struct puppeteerEvent_s {
	enum puppeteerEvent_e eventId;
	char data[MAX_EVENT_DATA];
	int read;
	struct timespec timestamp;
} puppeteerEvent_t;
/**/

typedef struct shmEventCtx_s {
	/* event sync */
	pthread_mutex_t event_m;
	pthread_cond_t event_c;
	/**/

	/* circular buffer */
	int eventBufferSize;
	int eventBufferStart;
	int eventBufferEnd;
	puppeteerEvent_t eventBuffer[MAX_EVENTS];
	/**/
} shmEventCtx_t;

#endif