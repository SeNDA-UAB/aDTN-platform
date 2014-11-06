#ifndef INC_DEBUG_H
#define INC_DEBUG_H

void log_nd_query(struct nd_query *nd_query, bool send);
void log_nd_beacon(struct nd_beacon *nd_beacon, bool send);
// void log_ix_req_list(struct ix_req_list *ix_req_list, bool send);
// void log_ix_list(struct ix_list *ix_list, bool send);
// void log_ix_req_data(struct ix_req_data *ix_req_data, bool send);
// void log_ix_data(struct ix_data *ix_data, bool send);
int log_nbs_table(struct nbs_list *nbs);

#endif