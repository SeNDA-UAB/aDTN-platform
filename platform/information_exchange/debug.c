void log_nd_query(struct nd_query *nd_query, bool send)
{
	if (send) {
		INFO_MSG("[SENT] ND_QUERY; platform_id: %s ", nd_query->platform_id);
	} else {
		INFO_MSG("[RECV] ND_QUERY; platform_id: %s ", nd_query->platform_id);
	}
}

void log_nd_beacon(struct nd_beacon *nd_beacon, bool send)
{
	if (send) {
		INFO_MSG("[SENT] ND_BEACON;  platform_id: %s platform_ip: %s platform_port: %d rit_announceables: %s", nd_beacon->platform_id, nd_beacon->platform_ip, nd_beacon->platform_port, nd_beacon->rit_announceables);
	} else {
		INFO_MSG("[RECV] ND_BEACON;  platform_id: %s platform_ip: %s platform_port: %d rit_announceables: %s", nd_beacon->platform_id, nd_beacon->platform_ip, nd_beacon->platform_port, nd_beacon->rit_announceables);
	}
}

int log_nbs_table(struct nbs_list *nbs)
{
	char log_msg[MAX_LOG_MSG];
	int off = 0;

	struct nb_info *current_nb, *tmp_nb;
	off += sprintf(log_msg, "[NBS] Neighbours table: ");

	pthread_rwlock_rdlock(&nbs->rwlock);
	HASH_ITER(hh, nbs->list, current_nb, tmp_nb) {
		pthread_rwlock_rdlock(&current_nb->rwlock);
		off += snprintf(log_msg + off, MAX_LOG_MSG - off, "ID: %s, LAST SEEN: %ld | ", current_nb->id, current_nb->last_seen);
		pthread_rwlock_unlock(&current_nb->rwlock);
	}
	pthread_rwlock_unlock(&nbs->rwlock);

	INFO_MSG(log_msg);

	return 0;
}