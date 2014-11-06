#ifndef INC_COMMON_NEIGHBOURS_H
#define INC_COMMON_NEIGHBOURS_H

#define MAX_NB_ID_LEN 255

int get_nbs(char **nbs_list); // Returns num of nbs
int get_nb_ip(const char *nb_id, /*out*/char **ip);
int get_nb_port(const char *nb_id, /*out*/int *port);
int get_nb_subscriptions(const char *nb_id, /*out*/ char **subscribed);

#endif
