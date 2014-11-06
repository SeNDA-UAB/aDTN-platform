#ifndef H_ADTNRITHLPER_INC
#define H_ADTNRITHLPER_INC

// #define ID_LEN 23

typedef struct _routing_exec_result {
	char *hops_list;
	int hops_list_l;
} routing_exec_result;

typedef struct _iter_info {
	char *nbs_list;
	short int nbs_list_l;
	short int position;
} nbs_iter_info;

typedef struct _nbs_iterator {
	char *(*start)(void);
	int *(*has_next)(void);
	char *(*next)(void);
	int (*num)(void);
	char *(*nb)(int);
} nbs_iterator;

extern nbs_iterator nbs;
extern char *dest;

void add_hop(const char *hop);
// void rm_hop();

#endif
