int init_world()
{
	if (pthread_rwlock_init(&world->nbs.rwlock, NULL) != 0)
		LOG_MSG(LOG__ERROR, true, "pthread_rwlock_init()");
	if (pthread_rwlock_init(&world->rwlock, NULL) != 0)
		LOG_MSG(LOG__ERROR, true, "pthread_rwlock_init()");

	//Init stats (values are sensed every 10s)
	world->last_ten_ewma_factor = 2.0 / (10.0 + 1.0);
	world->last_hundred_ewma_factor = 2.0 / (100.0 + 1.0);

	//Init stats
	if (reset_stat("num_nbs") != 0)
		LOG_MSG(LOG__ERROR, true, "Error reseting num_nbs stat");

	world->last_ten_ewma = get_stat("last_ten_ewma");
	if (world->last_ten_ewma < 0) {
		world->last_ten_ewma = 0;
		set_stat("last_ten_ewma", world->last_ten_ewma);
	}

	world->last_hundred_ewma = get_stat("last_hundred_ewma");
	if (world->last_hundred_ewma < 0) {
		world->last_hundred_ewma = 0;
		set_stat("last_hundred_ewma", world->last_hundred_ewma);
	}

	world->num_contacts = get_stat("num_contacts");
	if (world->num_contacts < 0) {
		world->num_contacts = 0;
		set_stat("num_contacts", world->num_contacts);
	}

	return 0;
}

int load_ritd_config(const char *config_file)
{
	int ret = 0;
	char *tmp_value = NULL;
	unsigned short use_default = 0;
	struct conf_list ritd_config = {0};

	//Load ritd section from adtn.ini
	if ((load_config("ritd", &ritd_config, config_file)) != 1)
		LOG_MSG(LOG__ERROR, false, "Error loading ritd config. Aborting execution.");

	use_default = 0;
	tmp_value = get_option_value("multicast_group", &ritd_config);
	if (tmp_value != NULL) {
		strncpy(world->multicast_group, tmp_value , sizeof(world->multicast_group));
		if (!ip_valid(world->multicast_group)) {
			LOG_MSG(LOG__ERROR, false, "Config error: Multicast IP format error. Using default value.");
			use_default = 1;
		}
	} else {
		use_default = 1;
	}
	if (use_default)
		strncpy(world->multicast_group, MULTICAST_GROUP, 16);

	use_default = 0;
	tmp_value = get_option_value("multicast_port", &ritd_config);
	if (tmp_value != NULL) {
		world->multicast_port = strtol((tmp_value), NULL, 10);
		if (world->multicast_port == 0 || world->multicast_port < 1024 || world->multicast_port > 65535) {
			LOG_MSG(LOG__ERROR, false, "Config error: Multicast port not valid. Using default value.");
			use_default = 1;
		}
	} else {
		use_default = 1;
	}
	if (use_default) {
		world->multicast_port = MULTICAST_PORT;
		use_default = 0;
	}

	tmp_value = get_option_value("announcement_period", &ritd_config);
	if (tmp_value != NULL) {
		world->announcement_period = strtol(tmp_value, NULL, 10);
		if (world->announcement_period == 0) {
			LOG_MSG(LOG__ERROR, false, "Config error: Announcement period can't be 0. Aborting execution.");
			ret = 1;
			goto end;
		}
	} else {
		LOG_MSG(LOG__ERROR, false, "Config error: Announcment period not specified. Aborting execution.");
		ret = 1;
		goto end;
	}

	tmp_value = get_option_value("announceable", &ritd_config);
	if (tmp_value != NULL) {
		world->announceable = strtol(tmp_value, NULL, 10);
	} else {
		LOG_MSG(LOG__INFO, false, "Announceable configuration option not specified. Using default option: announceable = 0");
		world->announceable = 0;
	}

	tmp_value = get_option_value("nb_ttl", &ritd_config);
	if (tmp_value != NULL)
		world->nbs_life = strtol(tmp_value, NULL, 10);
	else
		world->nbs_life = 2 * world->announcement_period;

	/* Internal option */
	tmp_value = get_option_value("test_mode", &ritd_config);
	if (tmp_value != NULL)
		world->test_mode = strtol(tmp_value, NULL, 10);
	else
		world->test_mode = 0;
	/**/

	free_options_list(&ritd_config);
end:

	return ret;
}

int free_world()
{

	struct nb_info *current_nb, *tmp_nb;

	HASH_ITER(hh, world->nbs.list, current_nb, tmp_nb) {
		HASH_DEL(world->nbs.list, current_nb);
		free(current_nb);
	}

	free(world);

	return 0;
}