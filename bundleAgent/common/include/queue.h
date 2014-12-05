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

#ifndef INC_COMMON_QUEUE_H
#define INC_COMMON_QUEUE_H

///Starts a connection with queueManager and returns a connection identifier >0
int queue_manager_connect(char* data_path, char *q_sockname);
///Close a connection with queueManager
inline void queue_manager_disconnect(int queue_socket, char* data_path, char* queue_sockname);
/// Inserts a Bundle in the queue.
int queue(const char *bundle_id, int queue_conn);
/// Empty full queue
void empty_queue(int queue_conn);
/// Extracts the first id from the queue and return it. If there are no bundles in queue returns NULL
char *dequeue(int queue_conn);
/// Return the number of bundles in queue
int bundles_in_queue(int queue_conn);

#endif