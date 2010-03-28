/******************************************************************************
 * eiwic - Extensible Ircbot Written In C
 * Copyright (C) Hannes Gr�uler <lordi@styleliga.org>
 *
 * connections.h: Header file for connections.c
 *****************************************************************************/
 
#ifndef _CONNECTIONS_H_INCLUDED
#define _CONNECTIONS_H_INCLUDED

struct eiwic_connection;
	
typedef int (*e_conn_established_f)(struct eiwic_connection *);
typedef int (*e_conn_read_f)(struct eiwic_connection *, char *, u_int);
typedef int (*e_conn_write_f)(struct eiwic_connection *);
typedef int (*e_conn_close_f)(struct eiwic_connection *);
typedef int (*e_conn_http_f)(char *http, void *data);
typedef int (*e_conn_incoming_f)(struct eiwic_connection *);

enum _CONN_STATUS {
	CONN_NONE,
	CONN_CONNECTING,
	CONN_ESTABLISHED,	
	CONN_LISTENING
};

struct eiwic_connection {
	u_char status;
	int sock;
	
	u_short port;
	u_long host;
	
	e_conn_established_f cbe;
	e_conn_incoming_f cbi;
	e_conn_read_f cbr;
	e_conn_write_f cbw;
	e_conn_close_f cbc;
	
	void *data;
	
	struct eiwic_plugin *plugin;
};

struct eiwic_http {
	struct eiwic_connection *conn;
		
	char file[255];
	char host[255];
	
	u_short port;
	
	e_conn_http_f callback;
	void *data;
	
	char *buffer;
};

typedef struct eiwic_connection CONN;
typedef struct eiwic_http HTTP;
	
typedef CONN *(pfn_conn_http_get)(char *, e_conn_http_f, void *);
typedef int (pfn_conn_unreg)(CONN *);
typedef int (pfn_conn_close)(CONN *);
typedef void (pfn_conn_destroy)(void *);
typedef CONN *(pfn_conn_connect)(char *hostname, u_short port,
	e_conn_established_f, e_conn_read_f, e_conn_write_f, e_conn_close_f);
typedef CONN *(pfn_conn_listen)(u_short port, int backlog,
	e_conn_incoming_f, e_conn_read_f, e_conn_write_f, e_conn_close_f);
typedef CONN *(pfn_conn_accept)(CONN *listening);

#ifndef EIWIC_MODULE
pfn_conn_http_get	conn_http_get;
pfn_conn_unreg		conn_unreg;
pfn_conn_close		conn_close;
pfn_conn_destroy	conn_destroy;
pfn_conn_connect	conn_connect;
pfn_conn_listen		conn_listen;
pfn_conn_accept		conn_accept;
#endif /* EIWIC_MODULE */
	
#endif /* _CONNECTIONS_H_INCLUDED */
