#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "common/include/constants.h"

#include "include/adtnrithelper.h"
#include "include/adtnrhelper.h"
#include "modules/include/exec.h"

/** Iterator **/
nbs_iterator nbs;
nbs_iter_info nbs_info = {
	.nbs_list = NULL,
	.nbs_list_l = 0,
	.position = 0
};

static char *start_nb()
{
	nbs_info.position = 0;
	return nbs_info.nbs_list + nbs_info.position * MAX_PLATFORM_ID_LEN;
}

static int has_next_nb()
{
	if (nbs_info.position == nbs_info.nbs_list_l)
		return 0;
	else
		return 1;
}


static char *nb (int i)
{
	if (i > nbs_info.nbs_list_l)
		return "";
	else
		return nbs_info.nbs_list + (i * MAX_PLATFORM_ID_LEN);
}

static char *next_nb()
{
	if (nbs_info.position == nbs_info.nbs_list_l)
		return "";
	else
		return nbs_info.nbs_list + (nbs_info.position++*MAX_PLATFORM_ID_LEN);
}

static int num_nbs()
{
	return nbs_info.nbs_list_l;
}

void init_env()
{
	nbs.start = (void *)start_nb;
	nbs.has_next = (void *)has_next_nb;
	nbs.next = (void *)next_nb;
	nbs.nb = (void *)nb;
	nbs.num = (void *)num_nbs;
}

/** **/

// Initialized before execution
char *dest;
routing_exec_result *r_result;

void add_hop(const char *hop)
{
	if (hop != NULL && *hop != '\0') {
		r_result->hops_list = realloc(r_result->hops_list, (r_result->hops_list_l + 1) * MAX_PLATFORM_ID_LEN * sizeof(char));
		strncpy(r_result->hops_list + (r_result->hops_list_l)*MAX_PLATFORM_ID_LEN, hop, MAX_PLATFORM_ID_LEN);
		r_result->hops_list_l++;
	}
}

// TODO
// void rm_hop();