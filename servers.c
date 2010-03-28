/******************************************************************************
 * eiwic - Extensible Ircbot Written In C
 * Copyright (C) Hannes Gräuler <lordi@styleliga.org>
 *
 * servers.c: This file contains functions to handle eiwic's internal list
 *            of IRC servers.
 *****************************************************************************/

#include "eiwic.h"
#include "settings.h"
#include "plugins.h"
#include "servers.h"

SERVER *server_add(char *host, u_short port, char ipv6)
{	
	SERVER *server;

	if (port == 0 || host == NULL)
		return NULL;

	server = malloc(sizeof(SERVER));
	bzero(server, sizeof(SERVER));

	strncpy(server->host, host, MAX_HOSTNAME_LEN - 1);
	server->host[MAX_HOSTNAME_LEN - 1] = '\0';
	server->port = port;
	
#ifdef IPV6
	server->ipv6 = ipv6;
#endif
	
	dlist_ins_next(&e_global->servers, dlist_tail(&e_global->servers), server);
	log(LOG_INIT_DEBUG, "Server %s (Port: %i) has been added.",
			server->host, server->port);

	return server;
}

void server_remove(SERVER *server)
{
	/* TODO */
}

void server_destroy(void *data)
{
	SERVER *serv = data;

	free(serv);
}
