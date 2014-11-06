#ifndef INC_EXEC_H
#define INC_EXEC_H

#include "modules/exec_c_helpers/include/adtnrhelper.h"
#include "hash.h"
#include "common/include/bundle.h"

char *get_so_path(const char *name, const code_type_e type);

int prepare_routing_code(const char *bundle_id, bundle_code_dl_s **bundle_code_dl);
int execute_routing_code(routing_dl_s *routing_dl, char *dest, routing_exec_result *result); //result must be initialized to 0s!

int prepare_no_helpers_code(code_type_e code_type, const char *bundle_id, bundle_code_dl_s **bundle_code_dl);
int execute_no_helpers_code(code_type_e code_type, void *code_dl, /*OUT*/ int *result);

int clean_bundle_dl(bundle_code_dl_s *b_dl);

#endif
