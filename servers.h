/******************************************************************************
 * eiwic - Extensible Ircbot Written In C
 * Copyright (C) Hannes Gräuler <lordi@styleliga.org>
 *
 * servers.h: This file contains function declarations for servers.c
 *****************************************************************************/
 
#ifndef _SERVERS_H_INCLUDED
#define _SERVERS_H_INCLUDED

#include <netinet/in.h>

#define MAX_HOSTNAME_LEN 255

struct eiwic_server {
	u_char host[MAX_HOSTNAME_LEN];
	u_short port;
	
	struct sockaddr_in sin;
	struct sockaddr_in lsin;

#ifdef IPV6	
	u_char ipv6;
	struct sockaddr_in6 sin6;
	struct sockaddr_in6 lsin6;
#endif
};

typedef struct eiwic_server SERVER;
	
typedef SERVER *(pfn_server_add)(char *, u_short, char);
typedef void (pfn_server_remove)(SERVER *);
typedef void (pfn_server_destroy)(void *);

pfn_server_add		server_add;
pfn_server_remove	server_remove;
pfn_server_destroy	server_destroy;

#endif /* _SERVERS_H_INCLUDED */
