
typedef struct _iter_info {
	char *nbs_list;
	short int nbs_list_l;
	short int position;
} nbs_iter_info;

typedef struct _routing_exec_result {
	char *hops_list;
	int hops_list_l;
} routing_exec_result;

void setNbsInfo(nbs_iter_info nbs);
int getDeliver();
int getForward();
int getRemove();
routing_exec_result *getForwardDests();

int execute_code(char *code, int code_l, char *code_state);
