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
