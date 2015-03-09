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

#ifndef INC_COMMON_EXECUTOR
#define INC_COMMON_EXECUTOR

#include <stdint.h>
#include <limits.h>

#include "bundle.h"
#include "constants.h"

typedef enum {
        ERROR = 0,
        OK
} status;

typedef enum {
        EXE = 0,
        RM
} petition_type_e;


struct _p_header {
        petition_type_e petition_type;
        code_type_e code_type;
        char bundle_id[NAME_MAX];
};

/* Petittions */
typedef struct _exec_ctx {
        char rit_branch[MAX_RIT_PATH];
} exec_ctx;

struct _petition {
        struct _p_header header;
        exec_ctx ctx;
};

/* Responses */
struct _exec_r_response {
        struct _p_header header;

        // Reponse result
        status correct;
        uint8_t num_hops;
        char hops_list[MAX_HOPS_LEN];
};

struct _simple_response {
        struct _p_header header;

        // Response result
        status correct;
        uint8_t result;
};

struct _rm_response {
        struct _p_header header;
        status correct;
};

union _response {
        struct _exec_r_response exec_r;
        struct _simple_response simple;
        struct _rm_response rm;
};
/**/

#endif