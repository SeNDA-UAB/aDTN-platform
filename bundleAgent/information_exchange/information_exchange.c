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

#include "include/information_exchange.h"

struct world_vars *world;

#include "hash.c"
#include "net.c"
#include "global.c"
#include "irm_helper.c"
#include "actions.c"
#include "packet_builder.c"
#include "packet_processor.c"
#include "debug.c"

/**
 * Cancellation clean-up handler
 * @param arg Double pointer to free
 */
static void free_double_pointer(void **arg)
{
	free(*arg);
}

/**
 * Cancellation clean-up handler
 * @param arg Multicast socket
 */
static void nb_monitor_cleaner(int *sock)
{
	leave_multicast_group(*sock, world->multicast_group, world->platform_ip);
	close(*sock);
}

/**
 * Cleans neighbours hash list periodically. Period defined in adtn.ini as neighbours_life
 */
static void clean_nbs()
{
	time_t actual_time = time(NULL);

	int deleted_nbs = 0, window_time = 0;
	struct nb_info *current_nb = NULL, *tmp = NULL;

	READ_LOCK(&world->nbs.rwlock);

	HASH_ITER(hh, world->nbs.list, current_nb, tmp) {
		if ((actual_time - current_nb->last_seen) >= world->nbs_life) {

			//If a nb has disappeared we can calculate the contact window and update stats
			window_time = current_nb->last_seen - current_nb->first_seen;
			LOG_MSG(LOG__INFO, false, "[NBS] NB: %s disappeared, window_time: %d", current_nb->id, window_time);

			WRITE_LOCK(&world->rwlock);
			world->last_ten_ewma = calculate_ewma(world->last_ten_ewma, window_time, world->last_ten_ewma_factor);
			world->last_hundred_ewma = calculate_ewma(world->last_hundred_ewma, window_time, world->last_hundred_ewma_factor);
			world->num_contacts++;
			UNLOCK_LOCK(&world->rwlock);

			if (set_stat("last_ten_ewma", world->last_ten_ewma) != 0)
				LOG_MSG(LOG__ERROR, false, "error setting last_ten_ewma stat");
			if (set_stat("last_hundred_ewma", world->last_hundred_ewma) != 0)
				LOG_MSG(LOG__ERROR, false, "error setting last_hundred_ewma stat");
			if (set_stat("num_contacts", world->num_contacts) != 0)
				LOG_MSG(LOG__ERROR, false, "error setting num_contacts stat");

			//Delete neighbour from hash table
			HASH_DEL(world->nbs.list, current_nb);
			free(current_nb);

			//Update stats
			world->nbs.num_nbs--;
			if (set_stat("num_nbs", world->nbs.num_nbs) != 0)
				LOG_MSG(LOG__ERROR, false, "error setting num_nbs stat");

			deleted_nbs++;
		}
	}

	if (deleted_nbs != 0) {
		LOG_MSG(LOG__INFO, false, "Neighbours cleaned. %d nbs disappeared", deleted_nbs);
		store_nbs_info_to_rit(&world->nbs);
	}
	UNLOCK_LOCK(&world->nbs.rwlock);

	pthread_exit(0);
}

/**
 * Processes a packet. Intended to work as a thread.
 * @param t_vars The variables required to process the packet.
 */
void process_packet(struct thread_vars *t_vars)
{
	struct reply_set reply = {0};

	switch (t_vars->recv_buffer[0]) {
	case ND_QUERY: {
		log_nd_query((struct nd_query *)t_vars->recv_buffer, 0);
		if (process_nd_query((struct nd_query *)t_vars->recv_buffer, t_vars->recv_buffer_l, t_vars->src_ip, &reply) != 0) {
			LOG_MSG(LOG__ERROR, false, "Error processing ND_QUERY");
			goto end;
		}

	}
	case ND_BEACON: {
		log_nd_beacon((struct nd_beacon *)t_vars->recv_buffer, 0);
		if (process_nd_beacon((struct nd_beacon *)t_vars->recv_buffer, t_vars->recv_buffer_l, t_vars->src_ip) != 0) {
			LOG_MSG(LOG__ERROR, false, "Error processing ND_BEACON");
			goto end;
		}
		break;
	}
	default:
		LOG_MSG(LOG__ERROR, false, "Unrecognized packet, type: %d", t_vars->recv_buffer[0]);
		goto end;
	}

	if (reply.lenght > 0) {
		//If destination address is not set, send to multicast address. If destination port is not set, send to multicast port.
		if (reply.dest == NULL || *reply.dest == '\0') {
			strncpy(reply.dest, world->multicast_group, 16);
		}
		if (reply.dest_port == 0) {
			reply.dest_port = world->multicast_port;
		}

		if (send_packet(world->sender_sock, reply.data, reply.lenght, reply.dest, reply.dest_port) < 0) {
			LOG_MSG(LOG__ERROR, false, "Can't send reply");
		}
		free(reply.data);
	}

end:
	free(t_vars->recv_buffer);
	free(t_vars);

	pthread_exit(0);
}


/**
 * Sends a presence beacon every announcement_period seconds.
 */
void presence_announcement(void)
{
	int nd_beacon_l = 0;
	struct sockaddr_in dest_mc_addr = {0};
	struct nd_beacon *nd_beacon = NULL;
	struct timespec announcement_period;

	announcement_period.tv_sec = world->announcement_period;
	announcement_period.tv_nsec = 0;

	dest_mc_addr.sin_family      = AF_INET;
	dest_mc_addr.sin_addr.s_addr = inet_addr(world->multicast_group);
	dest_mc_addr.sin_port        = htons(world->multicast_port);

	nd_beacon_l = build_nd_beacon(&nd_beacon);

	pthread_cleanup_push(free_double_pointer, &nd_beacon);
	for (;;) {
		ssize_t tlen = 0;

		//If there are new announceables we generate a new ND_BEACON
		if (world->rit.update_beacon) {
			free(nd_beacon);
			nd_beacon_l = build_nd_beacon(&nd_beacon);

			world->rit.update_beacon = false;
		}

		tlen = sendto(world->sender_sock, (void *) nd_beacon, nd_beacon_l, 0, (struct sockaddr *) &dest_mc_addr,  sizeof(dest_mc_addr));
		if (tlen != nd_beacon_l) {
			LOG_MSG(LOG__ERROR, true, "sendto()");
		} else {
			log_nd_beacon(nd_beacon, 1);
		}

		nanosleep(&announcement_period, NULL);
	}
	pthread_cleanup_pop(1);

	pthread_exit(0);
}

/**
 * Monitors the multicast socket
 * @param world Application context
 */
void nb_monitor(void)
{
	int mc_recv_sock = 0, src_addr_l = 0;
	uint8_t *recv_buffer = NULL;
	ssize_t recv_buffer_l = -1;
	pthread_t process_packet_t;
	struct thread_vars *t_vars = NULL;
	struct sockaddr_in src_addr = {0};

	src_addr_l = sizeof(src_addr);

	// Prepare socket
	if ((mc_recv_sock = create_and_bind_socket(world->multicast_port, world->multicast_group, 1)) == -1) {
		LOG_MSG(LOG__ERROR, false, "Can't bind %s:%d", world->multicast_group, world->multicast_port);
		goto end;
	}
	join_multicast_group(mc_recv_sock, world->multicast_group, world->platform_ip);

	//Clean nbs list
	reset_nbs_info_from_rit();

	pthread_cleanup_push(&nb_monitor_cleaner, &mc_recv_sock);
	for (;;) {
		recv_buffer = (unsigned char *)malloc(MAX_PAYLOAD_SIZE);
		recv_buffer_l = recvfrom(mc_recv_sock, recv_buffer, MAX_PAYLOAD_SIZE, 0, (struct sockaddr *) &src_addr, (socklen_t *) &src_addr_l);

		if (recv_buffer_l < 0) {
			LOG_MSG(LOG__ERROR, true, "recvfrom");
			free(recv_buffer);
			continue;
		}
		// Ignore if packet comes from ourselves
		if (strncmp((char *)recv_buffer + 1, world->platform_id, MAX_PLATFORM_ID_LEN) == 0 && !world->test_mode) {
			free(recv_buffer);
			continue;
		}

		// Init thread variables
		t_vars = (struct thread_vars *)malloc(sizeof(struct thread_vars));
		strncpy(t_vars->src_ip, inet_ntoa(src_addr.sin_addr), sizeof(t_vars->src_ip));
		t_vars->src_port = src_addr.sin_port;
		t_vars->recv_buffer = recv_buffer;
		t_vars->recv_buffer_l = recv_buffer_l;

		// Processes received beacon with a new thread
		if (pthread_create(&process_packet_t, NULL, (void *)process_packet, (void *)t_vars) != 0)
			LOG_MSG(LOG__ERROR, true, "pthread_create");
		if (pthread_detach(process_packet_t) != 0)
			LOG_MSG(LOG__ERROR, true, "pthread_detach");

	}
	pthread_cleanup_pop(1);

end:
	pthread_exit(0);
}

int main(int argc, char *argv[])
{
	int ret = 1, sig = 0;
	pthread_t presence_announcement_t, nb_monitor_t;
	struct common *shm = NULL;
	sigset_t blockedSigs = {{0}};

	// Cleaning functions timers
	struct itimerspec ts_nbs;
	struct timespec interval_nbs = {0}, first_nbs = {0};
	timer_t nbs_cleaner_timer = {0};
	struct sigevent sev_nbs;

	//Init aDTN process (shm and global option)
	if (init_adtn_process(argc, argv, &shm) != 0) {
		LOG_MSG(LOG__ERROR, false, "Error initializing information exchange. Aborting execution");
		goto end;
	}

	//Init world
	world = (struct world_vars *)calloc(1, sizeof(struct world_vars));

	if (init_world() != 0) {
		LOG_MSG(LOG__ERROR, false, "Error initilizing world vars.");
		goto end;
	}

	// Load ritd config to world
	if (load_ritd_config(shm->config_file) != 0) {
		LOG_MSG(LOG__ERROR, false, "Error laoding ritd config. Aborting execution");
		goto end;
	}

	// Shm options
	strncpy(world->platform_ip, shm->iface_ip, STRING_IP_LEN);
	strncpy(world->platform_id, shm->platform_id, MAX_PLATFORM_ID_LEN);
	world->platform_port = shm->platform_port;
	world->shm = shm;

	//Init sender socket
	world->sender_sock = create_and_bind_socket(0, world->platform_ip, 0);

	/*Start threads*/

	/* Block signals so the signals will not be received by the newly created threads */

	//Block signals
	if (sigaddset(&blockedSigs, SIGINT) != 0)
		LOG_MSG(LOG__ERROR, true, "sigaddset()");
	if (sigaddset(&blockedSigs, SIGTERM) != 0)
		LOG_MSG(LOG__ERROR, true, "sigaddset()");
	if (sigaddset(&blockedSigs, SIGUSR1) != 0)
		LOG_MSG(LOG__ERROR, true, "sigaddset()");
	if (sigaddset(&blockedSigs, SIGUSR2) != 0)
		LOG_MSG(LOG__ERROR, true, "sigaddset()");

	if (sigprocmask(SIG_BLOCK, &blockedSigs, NULL) == -1)
		LOG_MSG(LOG__ERROR, true, "sigprocmask()");
	/**/

	//Launch main threads
	if (pthread_create(&nb_monitor_t, NULL, (void *) nb_monitor, NULL) != 0)
		LOG_MSG(LOG__ERROR, true, "pthread_create()");
	if (pthread_create(&presence_announcement_t, NULL, (void *) presence_announcement, NULL) != 0)
		LOG_MSG(LOG__ERROR, true, "pthread_create()");

	//Neighbours cleaning
	if (world->nbs_life != 0) {
		interval_nbs.tv_sec = world->nbs_life;
		first_nbs.tv_sec = world->nbs_life;
		ts_nbs.it_value = first_nbs;
		ts_nbs.it_interval = interval_nbs;

		sev_nbs.sigev_notify = SIGEV_THREAD;
		sev_nbs.sigev_notify_function = clean_nbs;
		sev_nbs.sigev_notify_attributes = NULL;

		if (timer_create(CLOCK_REALTIME, &sev_nbs, &nbs_cleaner_timer) == -1)
			LOG_MSG(LOG__ERROR, true, "timer_create()");

		if (timer_settime(nbs_cleaner_timer, 0, &ts_nbs, NULL) == -1)
			LOG_MSG(LOG__ERROR, true, "timer_settime()");
	}

	//Wait synchronously for signals.
	//SIGINT and SIGTERM exit the program nicely
	//SIGUSR1 updates nbs info
	//SIGUSR2 updates beacon (for example if there are new announceables)
	siginfo_t si;
	for (;;) {
		sig = sigwaitinfo(&blockedSigs, &si);

		if (sig == SIGINT || sig == SIGTERM)
			break;

		if (sig == SIGUSR1)
			send_nd_query();

		if (sig == SIGUSR2) {
			WRITE_LOCK(&world->rwlock);
			world->rit.update_beacon = true;
			UNLOCK_LOCK(&world->rwlock);
		}

	}

	/*Terminate threads nicely and free main memory*/

	//Cancel threads
	if (pthread_cancel(presence_announcement_t) != 0)
		LOG_MSG(LOG__ERROR, true, "pthread_cancel()");
	if (pthread_cancel(nb_monitor_t) != 0)
		LOG_MSG(LOG__ERROR, true, "pthread_cancel()");

	//Wait until all threads exited nicely
	if (pthread_join(presence_announcement_t, NULL) != 0)
		LOG_MSG(LOG__ERROR, true, "pthread_join()");
	if (pthread_join(nb_monitor_t, NULL) != 0)
		LOG_MSG(LOG__ERROR, true, "pthread_join()");

	//Clean timers
	if (world->nbs_life != 0) {
		if (timer_delete(nbs_cleaner_timer) != 0)
			LOG_MSG(LOG__ERROR, true, "timer_delete()");
	}

	//Main clean
	if (close(world->sender_sock) != 0)
		LOG_MSG(LOG__ERROR, true, "close()");

end:
	if (reset_stat("num_nbs") != 0)
		LOG_MSG(LOG__ERROR, true, "Error resseting num_nbs stat");

	if (world)
		free_world();

	exit_adtn_process();

	return ret;
}
