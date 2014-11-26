int update_nbs_info(const char *nb_id, const char *nb_ip, const int nb_port, struct nbs_list *nbs)
{
	time_t recv_time = time(NULL);
	struct nb_info *nb = get_nb(nb_id, nbs);

	if (nb == NULL) {
		nb = init_nb(nb_id, nb_ip, nb_port, recv_time);
		add_nb(nb, nbs);
	} else {
		READ_LOCK(&nb->rwlock) ;
		nb->last_seen = recv_time;
		UNLOCK_LOCK(&nb->rwlock);
	}

	if (store_nbs_info_to_rit(nbs) != 0)
		LOG_MSG(LOG__ERROR, false, "Error storing updated nbs info into the RIT");

	return 0;
}