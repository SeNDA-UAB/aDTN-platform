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

float get_stat(const char *stat);
int set_stat(const char *stat, const float new_value);
int increase_stat(const char *stat);
int decrease_stat(const char *stat);
int reset_stat(const char* stat);
int remove_stat(const char *stat);
float calculate_ewma(const float old_ewma, const float new_value, const float factor);