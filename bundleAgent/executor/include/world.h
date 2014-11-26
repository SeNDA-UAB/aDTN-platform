//TODO: Populate world struct on init.
#ifndef H_WORL_INC
#define H_WORL_INC

#include "common/include/shm.h"

// Required for compiling the tests
#ifdef __cplusplus
extern "C" {
#endif


/* Global vars*/
typedef struct _world {
	char *bundles_path;
	char *objects_path;
	struct common *shm;
} world_s;

world_s world;
/**/

#ifdef __cplusplus
}
#endif

#endif
