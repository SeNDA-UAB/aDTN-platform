#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>

#include "platform/common/include/neighbours.h"

long time;

static void help(char *program_name)
{
    printf( "%s is part of the SeNDA aDTN platform\n"
            "Usage: %s [-t time]\n"
            "Supported options:\n"
            "       [-t | --time] \t\t\t\tTime in seconds between each neighbours display. Minimum time will be 1 second\n"
            "       [-h | --help]\t\t\t\tShows this help message\n"
            , program_name, program_name);
}

static void parse_arguments(int argc, char *const argv[])
{
    int opt = -1, option_index = 0;
    static struct option long_options[] =
    {
        {"help",        no_argument,            0,      'h'},
        {"time",        required_argument,      0,      't'}
    };
    while ((opt = getopt_long(argc, argv, "ht:", long_options, &option_index)))
    {
        switch (opt)
        {
        case 'h':
            help(argv[0]);
            exit(0);
        case 't':
            time = strtol(optarg, NULL, 10);
            break;
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

int main(int argc,  char *const *argv)
{
    char *nbs, *nb_ip, nb_id[MAX_PLATFORM_ID_LEN];
    int nbs_num, nb_port, i;

    parse_arguments(argc, argv);
    if (time <= 0)
    	time = 1;

    while (1)
    {
        nbs_num = get_nbs(&nbs);
        strcpy(nb_id, nbs);
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
        sleep(time);
    }
}