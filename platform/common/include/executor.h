#define MAX_HOPS_LEN 1024

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
struct _petition {
        struct _p_header header;
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