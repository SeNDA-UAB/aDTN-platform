int bind_socket(const int sock, const int port, const char *ip)
{
	int ret = 0;

	struct sockaddr_in recv_addr = {0};

	recv_addr.sin_family = AF_INET;
	recv_addr.sin_port = htons(port);
	if (ip != NULL)
		recv_addr.sin_addr.s_addr = inet_addr(ip);
	else
		recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if ((bind(sock, (struct sockaddr *) &recv_addr, sizeof(recv_addr))) < 0) {
		LOG_MSG(LOG__ERROR, true, "bind():");
		ret = 1;
	}

	return ret;
}

int create_and_bind_socket(const int port, const char *ip, const short reuse_addr)
{
	int sock, flag_on = 1;

	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		LOG_MSG(LOG__ERROR, true, "socket()");

	if (reuse_addr) {
		/* useful to have multiple information exchanges runnning at the same time in the same platform */
		if ((setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flag_on, sizeof(flag_on))) < 0) {
			LOG_MSG(LOG__ERROR, true, "setsockopt()");
		}
	}

	if (bind_socket(sock, port, ip) != 0) {
		close(sock);
		sock = -1;
	}

	return sock;
}

int join_multicast_group(const int mc_sock, const char *mc_group, const char *iface_ip )
{
	int ret = 0;
	struct ip_mreq mc_req = {{0}};

	/* construct an IGMP join request structure */
	mc_req.imr_multiaddr.s_addr = inet_addr(mc_group);
	if (iface_ip != NULL)
		mc_req.imr_interface.s_addr = inet_addr(iface_ip); //htonl(INADDR_ANY);
	else
		mc_req.imr_interface.s_addr = htonl(INADDR_ANY);

	/* send an ADD MEMBERSHIP message via setsockopt */
	if ((setsockopt(mc_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *) &mc_req, sizeof(mc_req))) < 0) {
		LOG_MSG(LOG__ERROR, true, "setsockopt()");
		ret = 1;
	}

	return ret;
}

int leave_multicast_group(const int mc_sock, const char *mc_group, const char *iface_ip)
{
	int ret = 0;
	struct ip_mreq mc_req = {{0}};

	/* construct an IGMP join request structure */
	mc_req.imr_multiaddr.s_addr = inet_addr(mc_group);
	mc_req.imr_interface.s_addr = inet_addr(iface_ip); //htonl(INADDR_ANY);

	/* send a DROP MEMBERSHIP message via setsockopt */
	if ((setsockopt(mc_sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void *) &mc_req, sizeof(mc_req))) < 0) {
		LOG_MSG(LOG__ERROR, true, "setsockopt()");
		ret = 1;
	}

	return ret;
}

int send_packet(int sender_sock, void *packet, int packet_l, char *dest_ip, int dest_port)
{
	struct sockaddr_in dest_mc_addr;  // socket address structure to send

	memset(&dest_mc_addr, 0, sizeof(dest_mc_addr));
	dest_mc_addr.sin_family      = AF_INET;
	dest_mc_addr.sin_addr.s_addr = inet_addr(dest_ip);
	dest_mc_addr.sin_port        = htons(dest_port);

	return sendto(sender_sock, (void *) packet, packet_l, 0, (struct sockaddr *) &dest_mc_addr,  sizeof(dest_mc_addr));

}

int send_nd_query()
{
	unsigned char nd_query = ND_QUERY;

	return send_packet(world->sender_sock, &nd_query, sizeof(nd_query), world->multicast_group, world->multicast_port);
}

int ip_valid(const char *ip)
{
	struct sockaddr_in sa;
	int result = inet_pton(AF_INET, ip, &(sa.sin_addr));
	return result != 0;
}