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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "bundleAgent/common/include/neighbours.h"
#include "bundleAgent/common/include/constants.h"
#include "bundleAgent/common/include/rit.h"
#include "bundleAgent/common/include/config.h"
#include "bundleAgent/common/include/log.h"
#include "lib/api/include/adtn.h"

struct _conf {
    struct common *shm;
    char *config_file;
    long interval;
};

struct _conf conf = {0};

static void help(char *program_name)
{
    printf( "%s is part of the SeNDA aDTN platform\n"
            "Usage: %s [options]\n"
            "Supported options:\n"
            "       [-f | --conf_file] config_file\t\tUse {config_file} config file instead of the default found at "DEFAULT_CONF_FILE_PATH"\n"
            "       [-i | --interval] \t\t\tInterval in seconds between each neighbours display. Minimum interval will be 1 second\n"
            "       [-h | --help]\t\t\t\tShows this help message\n"
            , program_name, program_name);
}

static void parse_arguments(int argc, char *const argv[])
{
    int opt = -1, option_index = 0;
    static struct option long_options[] =
    {   
        {"conf_file",       required_argument,          0,      'f'},
        {"interval",        required_argument,          0,      'i'},
        {"help",            no_argument,                0,      'h'}
    };
    while ((opt = getopt_long(argc, argv, "f:hi:", long_options, &option_index)))
    {
        switch (opt){
        case 'f':
            conf.config_file = strdup(optarg);
            break;
        case 'i':
            conf.interval = strtol(optarg, NULL, 10);
            break;
        case 'h':
            help(argv[0]);
            exit(0);
        case '?':           //Unexpected parameter
            help(argv[0]);
            exit(0);
         default:
            break;
        }

        if (opt == -1)
            break;
    }
}

void get_neighbours()
{
    char *nbs, *nb_ip, nb_id[MAX_PLATFORM_ID_LEN];
    int nbs_num, nb_port, i;

    while (1)
    {
        nbs_num = get_nbs(&nbs);
        if (nbs_num > 0)
        {
            printf("Neighbour list\n");
            printf("-------------------------------------------------------------------\n");
            for (i = 0; i < nbs_num; i++)
            {
                strcpy(nb_id, nbs + MAX_PLATFORM_ID_LEN * i);
                get_nb_ip(nb_id, &nb_ip);
                get_nb_port(nb_id, &nb_port);
                printf("id: %-*s \t ip: %s \t port: %d\n",20, nb_id, nb_ip, nb_port);
                free(nb_ip);
            }
            printf("-------------------------------------------------------------------\n\n");
            free(nbs);
        }
        else
        {
            printf("You have no neighbours\n");
        }
        sleep(conf.interval);
    }
}

int main(int argc,  char *const *argv)
{
    char *data_path = NULL, *rit_path = NULL;
    int shm_fd = 0, fd = 0, sig = 0, ret = 1;
    pthread_t get_neighbours_t;

    /* Check interval */
    parse_arguments(argc, argv);
    if (conf.interval <= 0)
        conf.interval = 1;

    /* Initialization */
    sigset_t blocked_sigs = {{0}};
    if (sigaddset(&blocked_sigs, SIGINT) != 0)
        LOG_MSG(LOG__ERROR, errno, "sigaddset()");
    if (sigaddset(&blocked_sigs, SIGTERM) != 0)
        LOG_MSG(LOG__ERROR, errno, "sigaddset()");
    if (sigprocmask(SIG_BLOCK, &blocked_sigs, NULL) == -1)
        LOG_MSG(LOG__ERROR, errno, "sigprocmask()");

    /* Redirect stderr to /dev/null to avoid that unwated error messages appear in terminal*/
    fd = open("/dev/null", O_RDWR);
    dup2(fd, 2);
    /**/

    if (conf.config_file == NULL)
    conf.config_file = DEFAULT_CONF_FILE_PATH;

    struct conf_list global_conf = {0};
    if (load_config("global", &global_conf, conf.config_file) != 1) {
        printf("Config file %s not found.\n", conf.config_file);
        goto end;
    }
    if ((data_path = get_option_value("data", &global_conf)) == NULL) {
        printf("Config file does not have the data key set.\n");
        goto end;
    }
    if (load_shared_memory_from_path(data_path, &conf.shm, &shm_fd, 0) != 0) {
        printf("Is platform running?\n");
        goto end;
    }

    /* Change the RIT path if it's not the default one */
    rit_path = malloc (strlen(data_path)+5);
    strcpy(rit_path, data_path);
    strcat(rit_path, "/RIT");

    if (rit_changePath(rit_path) == 1) {
        printf("Couldn't find RIT\n");
        goto end;
    }

    pthread_create(&get_neighbours_t, NULL, (void *)get_neighbours, NULL);

    /* Waiting for end signal */
    for (;;) {
        sig = sigwaitinfo(&blocked_sigs, NULL);

        if (sig == SIGINT || sig == SIGTERM) {
            break;
        }
    }
    /**/

    pthread_cancel(get_neighbours_t);
    pthread_join(get_neighbours_t, NULL);

    ret = 0;

    end:
        unmap_shared_memory(conf.shm);
        free(rit_path);
        return ret;
}