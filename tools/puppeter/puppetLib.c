#include <stdio.h>
#include <string.h>
#include <sys/mman.h> /* shm_open, mmap..*/
#include <sys/stat.h> /* For mode constants */
#include <fcntl.h> /* For O_* constants */

// Pupeteer
#include "include/puppeteerCommon.h"

//static void *puppetLibHandle;
static int puppeteerShmFd;
static shmEventCtx_t *puppeteerShm;

// Prepares shared memory (Shm is already initialized by the puppet master)
static int puppeteerInitShm(const char *shmName)
{
	puppeteerShmFd = shm_open(shmName, O_RDWR, S_IRWXU | S_IRWXG);
	if (puppeteerShmFd < 0){
		perror("shm_open()");
		return 1;
	}
	puppeteerShm  = (shmEventCtx_t *)mmap(NULL, sizeof(shmEventCtx_t), PROT_READ | PROT_WRITE, MAP_SHARED, puppeteerShmFd, 0);
	if (puppeteerShm == NULL){
		perror("mmap()");
		return 1;
	}

	puppeteerShm->eventBufferSize = MAX_EVENTS;

	return 0;
}	

// Locks shm.
static int puppeteerStartEvent()
{
	if (pthread_mutex_lock(&puppeteerShm->event_m) != 0){
		perror("pthread_mutex_lock()");
		return 1;
	} else {
		return 0;
	}
}

// Adds event to shared memory
static int puppeteerStoreEvent(puppeteerEvent_t *e)
{
	e->read = 0;
	clock_gettime(CLOCK_REALTIME, &e->timestamp);
	memcpy(&puppeteerShm->eventBuffer[puppeteerShm->eventBufferEnd], e, sizeof(*e));
	puppeteerShm->eventBufferEnd = (puppeteerShm->eventBufferEnd + 1) % puppeteerShm->eventBufferSize;
	if (puppeteerShm->eventBufferEnd == puppeteerShm->eventBufferStart){
		puppeteerShm->eventBufferStart = (puppeteerShm->eventBufferStart + 1) % puppeteerShm->eventBufferSize;
	}

	return 0;
}

// Notifies new event to puppet master
// Unlocks shm.
static int puppeteerEndEvent()
{
	if (pthread_cond_signal(&puppeteerShm->event_c) != 0){
		perror("pthread_cond_signal()");
		return 1;
	}

	if (pthread_mutex_unlock(&puppeteerShm->event_m) != 0){
		perror("pthread_mutex_lock()");
		return 1;
	} 

	return 0;
}


/* Pupeteer events */

static int puppeteerEventSimple(uint8_t data[MAX_EVENT_DATA])
{
	puppeteerEvent_t e;

	e.eventId = puppeteerEventSimpleId;
	memcpy(&e.data, data, MAX_EVENT_DATA);
	puppeteerStoreEvent(&e);

	return 0;
}

/**/