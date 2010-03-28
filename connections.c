/******************************************************************************
 * eiwic - Extensible Ircbot Written In C
 * Copyright (C) Hannes Gr�uler <lordi@styleliga.org>
 *
 * connections.c: Functions implementations for creating TCP/IP connections,
 *                listenening sockets and HTTP interface
 *****************************************************************************/

#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <errno.h>

#include "eiwic.h"
#include "connections.h"
#include "plugins.h"

CONN* conn_connect(char *hostname, u_short port,
	e_conn_established_f cbe,
	e_conn_read_f cbr,
	e_conn_write_f cbw,
	e_conn_close_f cbc)
{
	CONN *epc;
	struct sockaddr_in sin;
	int s, flags;
		
	epc = malloc(sizeof(CONN));
	bzero(epc, sizeof(CONN));
		
	epc->plugin = plugin;
	epc->cbc = cbc;
	epc->cbr = cbr;
	epc->cbw = cbw;
	epc->cbe = cbe;
	
	if (findip(hostname, (struct in_addr *)&epc->host) == -1)
	{
		log(LOG_DEBUG, "Unable to resolve host %s", inet_ntoa(*(struct in_addr*)&epc->host));
	}
		
	epc->port = port;
	
#ifdef VV_DEBUG
	log(LOG_VERBOSE_DEBUG, "Attempt to create connection to %s:%i", inet_ntoa(*(struct in_addr*)&epc->host), epc->port);
#endif

	bzero(&sin, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = epc->host;
	sin.sin_port = htons(epc->port);
	
	/* create us a socket.. */
	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	/* ..and set it to non-blocking */
	if (-1 == (flags = fcntl(s, F_GETFL, 0))) flags = 0;
		
   	if (-1 == (fcntl(s, F_SETFL, flags | O_NONBLOCK)))
	{
		log(LOG_ERROR, "connections.c: conn_connect: fcntl");
		return NULL;
	}

	/* finally connect in non-blocking mode: */
	if (connect(s, (struct sockaddr *) &sin, sizeof(struct sockaddr)) == -1)
	{
		if (errno != EINPROGRESS) /* Operation now in progress */
		{
			log(LOG_WARNING, "Connection failed to %s:%i",  inet_ntoa(*(struct in_addr*)&epc->host), epc->port);
			return NULL;
		}
	}

	epc->sock = s;
	epc->status = CONN_CONNECTING;
	
	log(LOG_DEBUG, "Connecting non-blocking to %s:%i", inet_ntoa(*(struct in_addr*)&epc->host), epc->port);
	dlist_ins_next(&e_global->connections, e_global->connections.tail, epc);
		
	return epc;
}

CONN *conn_listen(u_short port, int backlog, e_conn_incoming_f cbi,
	e_conn_read_f cbr,
	e_conn_write_f cbw,
	e_conn_close_f cbc)
{
	CONN *epc;
	struct sockaddr_in sin;
	int s, flags;
		
	epc = malloc(sizeof(CONN));
	bzero(epc, sizeof(CONN));
		
	epc->plugin = plugin;
	epc->cbi = cbi;
	epc->cbr = cbr;
	epc->cbw = cbw;
	epc->cbc = cbc;
	
	/* create us a socket.. */
	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	/* ..and set it to non-blocking */
	if (-1 == (flags = fcntl(s, F_GETFL, 0))) flags = 0;
		
   	if (-1 == (fcntl(s, F_SETFL, flags | O_NONBLOCK)))
	{
		log(LOG_ERROR, "connections.c: conn_listen: fcntl");
		return NULL;
	}

	bzero(&sin, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(port);
	
	if (bind(s, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) == -1)
	{
		log(LOG_ERROR, "connections.c: conn_listen: bind");
		return NULL;
	}

	/* finally listen in non-blocking mode: */
	if (listen(s, backlog) == -1)
	{
		if (errno != EINPROGRESS) /* Operation now in progress */
		{
			log(LOG_WARNING, "Unable to create listening socket");
			return NULL;
		}
	}

	epc->sock = s;
	epc->status = CONN_LISTENING;
	
	log(LOG_DEBUG, "Listening socket has been set up on port %i", port);
	dlist_ins_next(&e_global->connections, e_global->connections.tail, epc);
			
	return epc;
}

CONN *conn_accept(CONN *lis)
{
	CONN *epa;
	struct sockaddr_in sin;
	int s, flags, sin_len = sizeof(struct sockaddr_in);
		
	epa = malloc(sizeof(CONN));
	bzero(epa, sizeof(CONN));
		
	epa->plugin = lis->plugin;
	epa->cbr = lis->cbr;
	epa->cbw = lis->cbw;
	epa->cbc = lis->cbc;
	
	/* accept incoming connection.. */
	if (-1 == (s = accept(lis->sock, (struct sockaddr *)&sin, &sin_len)))
	{
		log(LOG_ERROR, "connections.c: conn_accept: accept");
		return NULL;
	}

	/* ..and set it to non-blocking */
	if (-1 == (flags = fcntl(s, F_GETFL, 0))) flags = 0;
		
   	if (-1 == (fcntl(s, F_SETFL, flags | O_NONBLOCK)))
	{
		log(LOG_ERROR, "connections.c: conn_accept: fcntl");
		return NULL;
	}
	
	epa->host = sin.sin_addr.s_addr;
	epa->sock = s;
	epa->status = CONN_ESTABLISHED;

	log(LOG_DEBUG, "Connection succesfully accepted from remote port %d", htons(sin.sin_port));
	dlist_ins_next(&e_global->connections, e_global->connections.tail, epa);
	
	return epa;
}


int conn_close(CONN *con)
{
	plog(con->plugin, LOG_DEBUG, "Connection closed to %s:%i", inet_ntoa(*(struct in_addr*)&con->host), con->port);
	
	if (con->cbc)
	{
		e_global->jump_back = 1;
		if (sigsetjmp(e_global->jmp_past, 1) == 0)
		{
			plugin = con->plugin;
			con->cbc(con);
			plugin = NULL;
		}
		else
		{
			e_global->jump_back = 0;
			SEGWARN(con->plugin, "e_conn_close_f");
		}
		e_global->jump_back = 0;
	}

	conn_unreg(con);	
	return 1;
}

int conn_unreg(CONN *conn)
{
	dlist_remove(&e_global->connections, dlist_find(&e_global->connections, conn), NULL);	
	conn_destroy(conn);

	return 1;
}

void conn_destroy(void *data)
{
	CONN *conn = data;	

	shutdown(conn->sock, 2);
	close(conn->sock);
	free(conn);

}

int http_on_connect(CONN *conn)
{
	char buf[1024];
	
	sprintf(buf, "GET %s HTTP/1.0\r\nHost: %s:%i\r\n\r\n", ((HTTP *)conn->data)->file, ((HTTP *)conn->data)->host, ((HTTP *)conn->data)->port);
	send(conn->sock, buf, strlen(buf), 0);
	
	return 0;
}

int http_on_read(CONN *conn, char*buf, u_int len)
{
	HTTP *http;
	int old_len;
	
	http = conn->data;
	old_len = http->buffer?strlen(http->buffer):0;
	
	http->buffer = realloc(http->buffer, old_len + len + 1);
	memcpy(http->buffer + old_len, buf, len);
	http->buffer[old_len + len] = 0;
	return 0;
}

int http_on_close(CONN *conn)
{
	HTTP *http;
	char *code;

	http = conn->data;

	if (http->buffer)
	{	
		log(LOG_DEBUG, "HTTP request (%s %s) returned %d bytes: %.14s...",
			http->host,
			http->file,
			strlen(http->buffer),
			http->buffer);
			
	
		code = strstr(http->buffer, "\r\n\r\n");
		if (code) {
			*code = 0;
			code += 4;
		}
		else {
			code = http->buffer;
		}
		
		
		if (http->callback != NULL)
			http->callback(code, http->data);

		if (http->buffer) free(http->buffer);
	}
	else {
		if (http->callback != NULL)
			http->callback(NULL, http->data);
	}
	return 0;		
}

CONN *conn_http_get(char *url, e_conn_http_f cb, void *data)
{
	char host[128];
	char *file, *t;
	HTTP *http;
	CONN *c;
	
	if (strncasecmp(url, "http://", 7) != 0)
		return NULL;
	
	file = strchr(url + 7, '/');
	strncpy(host, url + 7, file - (url + 7));
	host[file - (url + 7)] = 0;
	if ((t = strchr(host, ':')) != NULL)
		*t = 0;
	c = conn_connect(host, t?atoi(t+1):80, http_on_connect, http_on_read, NULL, http_on_close);
	if (c)
	{	
		http = malloc(sizeof(HTTP));
		
		c->data = http;
		http->conn = c;
		strcpy(http->file, file);
		strcpy(http->host, host);
		http->buffer = NULL;
		http->callback = cb;
		http->data = data;
		http->port = t?atoi(t+1):80;

		return c;
	}
	
	return NULL;
}
