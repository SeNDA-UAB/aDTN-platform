/*
* Copyright (c) 2015 SeNDA
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

#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "bundleAgent/common/include/constants.h"
#include "bundleAgent/common/include/config.h"

#include "lib/api/include/adtn.h"

#define DEFAULT_LIFETIME 30
#define DEFAULT_ADTN_PORT 65530

static void help(char *program_name) {
  printf( "%s is part of the SeNDA aDTN platform\n"
          "Usage: %s -m 'Message to send' -d 'node101' [options]\n"
          "Required options:\n"
          "       [-m | --message] message\t\t\tMessage to send\n"
          "       [-d | --destination] destination\t\t\tSet the destination of the message.\n"
          "Supported options:\n"
          "       [-P | --port] port\t\t\tDestination adtn port (default 65530).\n"
          "       [-f | --conf_file] config_file\t\tUse {config_file} config file instead of the default found at "DEFAULT_CONF_FILE_PATH"\n"
          "       [-l | --lifetime] lifetime\t\tSet {lifetime} seconds of lifetime of the message (default 30s).\n"
          "       [-h | --help]\t\t\t\tShows this help message.\n"
          , program_name, program_name);
}

int main(int argc, char *const *argv) {
  int opt = -1, option_index = 0, shm_fd = 0, sock = 0;
  char *config_file = NULL;
  char *destination = NULL;
  char *message = NULL;
  char *data_path = NULL;
  struct common *shm;
  long lifetime = 0;
  int dest_adtn_port = 0;
  sock_addr_t local;
  sock_addr_t dest;

  static struct option long_options[] = {
    {"destination", required_argument, 0, 'd'},
    {"conf_file", required_argument, 0, 'f'},
    {"message", required_argument, 0, 'm'},
    {"lifetime", required_argument, 0, 'l'},
    {"port", required_argument, 0, 'P'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
  };

  while ((opt = getopt_long(argc, argv, "m:d:P:f:l:h", long_options,
                            &option_index))) {
    switch (opt) {
      case 'd':
        destination = strdup(optarg);
        break;
      case 'f':
        config_file = strdup(optarg);
        break;
      case 'm':
        message = strdup(optarg);
        break;
      case 'l':
        lifetime = strtol(optarg, NULL, 10);
        break;
      case 'P':
        dest_adtn_port = strtol(optarg, NULL, 10);
        break;
      case 'h':
      case '?':
        help(argv[0]);
        exit(0);
      default:
        break;
    }

    if (opt == -1)
      break;
  }
  if (destination == NULL || message == NULL) {
    help(argv[0]);
    exit(0);
  }

  if (config_file == NULL)
    config_file = DEFAULT_CONF_FILE_PATH;
  if (lifetime <= 0)
    lifetime = DEFAULT_LIFETIME;
  if (dest_adtn_port <= 0)
    dest_adtn_port = DEFAULT_ADTN_PORT;

  struct conf_list global_conf = {0};
  if (load_config("global", &global_conf, config_file) != 1) {
    printf("Config file %s not found.\n", config_file);
    exit(1);
  }
  if ((data_path = get_option_value("data", &global_conf)) == NULL) {
    printf("Config file does not have the data key set.\n");
    exit(1);
  }
  if (load_shared_memory_from_path(data_path, &shm, &shm_fd, 0) != 0) {
    printf("Is platform running?\n");
    exit(1);
  }

  local.id = strdup(shm->platform_id);
  local.adtn_port = dest_adtn_port;

  dest.id = destination;
  dest.adtn_port = dest_adtn_port;

  sock = adtn_socket(shm->config_file);

  if (adtn_bind(sock, &local) != 0) {
    printf("adtn_bind could not be performed.\n");
    exit(1);
  }
  if (adtn_setsockopt(sock, OP_LIFETIME, &lifetime) != 0)
    exit(1);

  if (adtn_sendto(sock, message, strlen(message), dest) != strlen(message)) {
    if (errno == EMSGSIZE) {
      printf("Error sending, message too big.\n");
    } else {
      printf("Error sendinf message.\n");
    }
    adtn_shutdown(sock);
    exit(1);
  }
  adtn_shutdown(sock);
  return 0;
}

