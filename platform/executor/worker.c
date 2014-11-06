#define SET_RESPAWN_NOTIFICATION(child_id)                                      \
	do {                                                                        \
		pthread_mutex_lock(params->respawn_child_mutex[child_id]);              \
		params->respawn_child[child_id] = 1;                                    \
		pthread_mutex_unlock(params->respawn_child_mutex[child_id]);            \
	} while(0);                                                                 \
	 
#define UNSET_RESPAWN_NOTIFICATION(child_id)                                    \
	do {                                                                        \
		pthread_mutex_lock(params->respawn_child_mutex[child_id]);              \
		params->respawn_child[child_id] = 0;                                    \
		pthread_mutex_unlock(params->respawn_child_mutex[child_id]);            \
	} while(0);                                                                 \
	 
/* Cleanup handlers */
void kill_child(int *pid)
{
	if (*pid != 0) {
		INFO_MSG("Child %d killed", *pid);
		kill(*pid, SIGTERM);
	}
}

void close_socekt(int *s)
{
	close(*s);
}

void clean()
{
	clean_all_bundle_dl();
	exit_adtn_process();

	_exit(0);
}
/**/

void executor_process(int sv)
{
	size_t response_header_l = 0;
	pid_t pid = getpid();
	response_header_l = sizeof(struct _p_header);
	sigset_t unblocked_sigs = {{0}};

	// Allow termination of the process
	if (sigaddset(&unblocked_sigs, SIGTERM) != 0)
		err_msg(true, "sigaddset()", errno);
	if (sigaddset(&unblocked_sigs, SIGINT) != 0)
		err_msg(true, "sigaddset()", errno);
	sigprocmask(SIG_UNBLOCK, &unblocked_sigs, NULL);

	// if (signal(SIGTERM, clean) == SIG_ERR)
	//  err_msg(true, "signal()");

	for (;;) {
		ssize_t ret_tmp = 0;
		size_t p_l = 0;
		struct _child_exec_petition p = {{0}};
		union _response r = {{{0}}};

		/* Routing code execution */
		unsigned r_l = 0;
		routing_exec_result r_result = {0};
		/**/

		/* No helpers execution*/
		int n_result = 0;
		/**/

		// Wait for petition
		errno = 0;
		p_l = recv(sv, &p, sizeof(p), 0);
		INFO_MSG("Child %u: Received petition for executing bundle %s", pid, p.bundle_id);

		if (p_l != sizeof(p)) {
			err_msg(false, "Received incorrect petition");
			continue;
		}

		switch (p.code_type) {
		case ROUTING_CODE:
			INFO_MSG("Child %u: Executing bundle %s with code_dl loaded in address %p", pid, p.bundle_id, p.routing_dl);

			if (execute_routing_code(p.routing_dl, p.dest, &r_result) != 0) {
				err_msg(false, "Error executing code (type %d) from bundle %s.", p.code_type, p.bundle_id);
				goto end;
			}
			if ((r_result.hops_list_l * MAX_ID_LEN) > MAX_HOPS_LEN) {
				err_msg(false, "Response too long (too many hops?). Probably something went worng.");
				goto end;
			}

			// Prepare response
			r_l = sizeof(union _response);
			r.exec_r.correct = OK;
			r.exec_r.num_hops = r_result.hops_list_l;
			memcpy(&r.exec_r.hops_list, r_result.hops_list, r_result.hops_list_l * MAX_ID_LEN);

			free(r_result.hops_list);

			break;
		case LIFE_CODE: // Simple responses
		case PRIO_CODE:
			switch (p.code_type) {
			case LIFE_CODE:
				INFO_MSG("Child %u: Executing bundle %s with code_dl loaded in address %p", pid, p.bundle_id, p.life_dl);
				ret_tmp = execute_no_helpers_code(p.code_type, p.life_dl, &n_result);
				break;
			case PRIO_CODE:
				INFO_MSG("Child %u: Executing bundle %s with code_dl loaded in address %p", pid, p.bundle_id, p.prio_dl);
				ret_tmp = execute_no_helpers_code(p.code_type, p.prio_dl, &n_result);
				break;
			default:
				goto end;
			}

			if (ret_tmp != 0) {
				err_msg(false, "Error executing code (type %d) from bundle %s.", p.code_type, p.bundle_id);
				goto end;
			}

			r_l = sizeof(union _response);
			r.simple.correct = OK;
			r.simple.result = n_result;
			break;
		default:
			err_msg(false, "Invalid code type received");
			goto end;
		}


end:
		// Copy header to response
		memcpy(&r.exec_r, &p, response_header_l);

		// Return result
		if (send(sv, &r, r_l, 0) != r_l) {
			err_msg(true, "Can't send response.");
		} else {
			INFO_MSG("Child %u: Result of bundle %s execution has been sent to the parent", pid, p.bundle_id);
		}

		// Wait until parent resumes execution
		if (raise(SIGSTOP) != 0)
			err_msg(true, "raise()");
	}
}

/* Child functions */
ssize_t is_child_alive(int pid)
{
	ssize_t ret = 0;

	if (pid != 0) {
		errno = 0;
		kill(pid, 0);
		if (errno != ESRCH) {
			ret = 1;
		}
	}

	return ret;
}

ssize_t restart_child(unsigned child_pid, int old_sv[2], int new_sv[2])
{
	ssize_t new_child_pid = -1;
	int status;
	/*
	NOTE: New sockets are created to avoid that a dying process receives a new petition before dying
	 */

	// Close old sockets
	if (old_sv[0] != 0 && close(old_sv[0]) != 0)
		err_msg(true, "close()");
	if (old_sv[1] != 0 && close(old_sv[1]) != 0)
		err_msg(true, "close()");

	// Create new sockets
	if (socketpair(AF_UNIX, SOCK_DGRAM, 0, new_sv) != 0) {
		err_msg(true, "socketpair()");
		goto end;
	}

	// Kill child if existed
	if (is_child_alive(child_pid)) {
		kill(child_pid, SIGTERM);
		wait(&status);
		INFO_MSG("Child %u killed", child_pid);
	}

	// Create new child
	if ((new_child_pid = fork()) == 0) {        // Child
		executor_process(new_sv[0]);
	}
end:
	return new_child_pid;
}
/**/

ssize_t load_code(const char *bundle_id, code_type_e code_type, /*OUT*/ bundle_code_dl_s **bundle_code_dl)
{
	ssize_t ret = 0;

	switch (code_type) {
	case ROUTING_CODE:
		ret = prepare_routing_code(bundle_id, bundle_code_dl);
		break;
	case PRIO_CODE:
	case LIFE_CODE:
		ret = prepare_no_helpers_code(code_type, bundle_id, bundle_code_dl);
		break;
	default:
		ret = -1;
	}

	return ret;
}

ssize_t notify_child_respawn(worker_params *params)
{
	unsigned short i = 0;

	for (i = 0; i < POOL_SIZE; i++) {
		SET_RESPAWN_NOTIFICATION(i);
	}

	return 0;
}

ssize_t remove_bundle(const char *bundle_id)
{
	char *path_to_delete = NULL;
	ssize_t ret = 0;
	bundle_code_dl_s *b_dl = NULL;

	if ((b_dl = bundle_dl_find(bundle_id)) == NULL) {
		ret = 1;
		goto end;
	}

	if (b_dl->dls.routing != NULL) {
		path_to_delete = get_so_path(bundle_id, ROUTING_CODE);
		if (unlink(path_to_delete) != 0) {
			err_msg(1, "Error removing %s", path_to_delete);
			ret |= 1;
		}
		free(path_to_delete);
	}

	if (b_dl->dls.life != NULL) {
		path_to_delete = get_so_path(bundle_id, LIFE_CODE);
		if (unlink(path_to_delete) != 0) {
			err_msg(1, "Error removing %s", path_to_delete);
			ret |= 1;
		}
		free(path_to_delete);
	}

	if (b_dl->dls.prio != NULL) {
		path_to_delete = get_so_path(bundle_id, PRIO_CODE);
		if (unlink(path_to_delete) != 0) {
			err_msg(1, "Error removing %s", path_to_delete);
			ret |= 1;
		}
		free(path_to_delete);
	}

	if (clean_bundle_dl(b_dl) != 0)
		ret |= 1;
	bundle_dl_remove(b_dl);
	free(b_dl);

end:

	return ret;

}


void worker_thread(worker_params *params)
{
	ssize_t ret = 0;
	size_t response_header_l = 0;
	pid_t child_pid = 0;
	int sv[2] = {0};
	fd_set readfds, safe;
	enum exec_state state;

	pthread_cleanup_push(kill_child, &child_pid);
	pthread_cleanup_push(free, params);

	response_header_l = sizeof(struct _p_header);

	// Prepare child comm.
	if (socketpair(AF_UNIX, SOCK_DGRAM, 0, sv) != 0) {
		ret = 1;
		err_msg(true, "socketpair()");
		goto end;
	}

	pthread_cleanup_push(close_socekt, &sv[0]);
	pthread_cleanup_push(close_socekt, &sv[1]);

	// Prepare socket set
	FD_ZERO(&readfds);
	FD_SET(params->main_socket, &readfds);
	safe = readfds;

	while (select (FD_SETSIZE, &readfds, NULL, NULL, NULL)) {
		ssize_t recv_l = 0, r_l = 0, p_l = 0;

		int status = 0;
		unsigned short cont = 1;

		struct sockaddr_un src_addr = {0};
		socklen_t src_addr_l = sizeof(src_addr);

		bundle_code_dl_s *bundle_code_dl = NULL;
		struct _child_exec_petition exec_p;
		struct _petition p;
		union _response r = {{{0}}};

		// Received execution petition
		if (FD_ISSET(params->main_socket, &readfds)) {
			INFO_MSG("Thread %d: Execution command received", params->thread_num);

			state = RECV_PETITION;
			while (cont) {
				switch (state) {
				case RECV_PETITION:
					p_l = recvfrom(params->main_socket, &p, sizeof(p), MSG_DONTWAIT, (struct sockaddr *)&src_addr, (socklen_t *)&src_addr_l);
					if (p_l <= 0 && errno != EWOULDBLOCK) {
						err_msg(true, "Thread %d: Error reading pettiion", params->thread_num);
						state = END;
						continue;
					} else if (p_l < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
						state = END;
						continue;
					}
					INFO_MSG("Thread %d: Execution for bundle %s received from %s", params->thread_num, p.header.bundle_id, src_addr.sun_path);

					if (p.header.petition_type == RM)
						state = RM_BUNDLE;
					else if (p.header.petition_type == EXE)
						state = LOAD_CODE;
					break;

				case RM_BUNDLE:
					if (remove_bundle(p.header.bundle_id) != 0) {
						err_msg(0, "Thread %d: Can't remove bundle %s", params->thread_num, p.header.bundle_id);
						state = RM_ERROR;
					} else {
						INFO_MSG("Thread %d: Removed bundle %s", params->thread_num, p.header.bundle_id);
						state = RM_OK;
					}

					break;
				case LOAD_CODE:
					pthread_mutex_lock(params->preparing_exec);
					ret = load_code(p.header.bundle_id, p.header.code_type, &bundle_code_dl);
					pthread_mutex_unlock(params->preparing_exec);

					switch (ret) {
					case 0:
						pthread_mutex_lock(params->respawn_child_mutex[params->thread_num]);
						if (params->respawn_child[params->thread_num])
							state = RESPAWN_CHILD;
						else
							state = EXEC_CODE;
						pthread_mutex_unlock(params->respawn_child_mutex[params->thread_num]);
						break;
					case 1:
						state = NOTIFY_CHILD_RESPAWN_AND_RESPAWN;
						break;
					default:
						err_msg(false,
						        "Thread %d: Error loading code from bundle %s.",
						        params->thread_num,
						        p.header.bundle_id);
						state = EXEC_ERROR;
						break;
					}
					break;
				case NOTIFY_CHILD_RESPAWN_AND_RESPAWN:
					notify_child_respawn(params);
				case RESPAWN_CHILD:
					pthread_mutex_lock(params->respawn_child_mutex[params->thread_num]);
					child_pid = restart_child(child_pid, sv, sv);
					if (child_pid == -1) {
						err_msg(false, "Thread %d: Error creating a new child", params->thread_num);
					} else {
						INFO_MSG("Child %u created", child_pid)
					}
					params->respawn_child[params->thread_num] = 0;
					pthread_mutex_unlock(params->respawn_child_mutex[params->thread_num]);

					state = EXEC_CODE;
					break;
				case EXEC_CODE:
					bzero(&exec_p, sizeof(exec_p));
					strncpy(exec_p.bundle_id, p.header.bundle_id, sizeof(exec_p.bundle_id));
					exec_p.code_type = p.header.code_type;
					void *code_dl = NULL;

					switch (p.header.code_type) {
					case ROUTING_CODE:
						strncpy(exec_p.dest, bundle_code_dl->info.dest, sizeof(exec_p.dest));
						exec_p.routing_dl = bundle_code_dl->dls.routing->dl;
						code_dl = bundle_code_dl->dls.routing->dl;
						break;
					case PRIO_CODE:
						exec_p.prio_dl = bundle_code_dl->dls.prio->dl;
						code_dl = bundle_code_dl->dls.prio->dl;
						break;
					case LIFE_CODE:
						exec_p.life_dl = bundle_code_dl->dls.life->dl;
						code_dl = bundle_code_dl->dls.life->dl;
						break;
					}

					if (send(sv[1], &exec_p, sizeof(exec_p), 0) != sizeof(exec_p)) {
						err_msg(true, "Thread %d: send()", params->thread_num);
					} else {
						INFO_MSG("Thread %d: Petition for executing bundle %s with code_dl in address %p sent", params->thread_num, p.header.bundle_id, code_dl);

						// Wait for child to pause or terminate
						waitpid(child_pid, &status, WUNTRACED);
					}

					if (WIFSTOPPED(status)) { // Child has executed the code correctly and it is still alive. Response is ready to be read.
						bzero(&r, sizeof(r));
						recv_l = recv(sv[1], &r, sizeof(r), 0);
						if (recv_l <= 0 || recv_l != sizeof(r)) {
							err_msg(true, "Thread %d: Error reading response from executor process.", params->thread_num);
							state = EXEC_ERROR;
						} else {
							INFO_MSG("Thread %d: Result received from child", params->thread_num);
							state = EXEC_OK;
						}
						kill(child_pid, SIGCONT); // Continue child execution
					} else if (WIFSIGNALED(status)) {
						err_msg(false, "Thread %d: Error executing code. Child has terminated.", params->thread_num);

						SET_RESPAWN_NOTIFICATION(params->thread_num);

						state = EXEC_ERROR;
					} else {
						err_msg(false, "Thread %d: Error executing code. Uknown error", params->thread_num);

						SET_RESPAWN_NOTIFICATION(params->thread_num);

						state = EXEC_ERROR;
					}
					break;
				case EXEC_OK:
					memcpy(&r, &p, response_header_l);
					state = SEND_RESULT;
					break;
				case EXEC_ERROR:
					memcpy(&r, &p, response_header_l);
					r.exec_r.correct = ERROR;
					state = SEND_RESULT;
					break;
				case RM_OK:
					memcpy(&r, &p, response_header_l);
					r.rm.correct = OK;
					state = SEND_RESULT;
					break;
				case RM_ERROR:
					memcpy(&r, &p, response_header_l);
					r.rm.correct = ERROR;
					state = SEND_RESULT;
					break;
				case SEND_RESULT:
					r_l = sizeof(r);
					if (sendto(params->main_socket, &r, r_l, 0, (struct sockaddr *)&src_addr, (socklen_t)sizeof(src_addr)) != r_l)
						err_msg(true, "Thread %d: Error sending result to client;", params->thread_num);
					else
						INFO_MSG("Thread %d: Result send to %s", params->thread_num, src_addr.sun_path);
					state = END;
					break;
				case END:
					cont = 0;
					break;
				}
			}
		}
		readfds = safe;
	}
end:

	pthread_cleanup_pop(1);
	pthread_cleanup_pop(1);
	pthread_cleanup_pop(1);
	pthread_cleanup_pop(1);
	pthread_exit(&ret);
}
