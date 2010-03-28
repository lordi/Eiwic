#include "servers.h"
#include "connections.h"

typedef OUTPUT *(pfn_output_open)(int, ...);
typedef char *(pfn_output_tostring)(OUTPUT *);
typedef int (pfn_output_print)(OUTPUT *, char *);
typedef int (pfn_output_printl)(OUTPUT *, char *, int);
typedef int (pfn_output_printf)(OUTPUT *, char *, ...);
typedef int (pfn_output_close)(OUTPUT *);
typedef void (pfn_output_destroy)(void *);

MODLINK {
	SETTING* (*set_add)(int, char *, ...);
	SETTING* (*set_find)(char *);
	void (*set_set)(SETTING *, ...);
	int (*set_is)(char *); /* retrieves SET_BOOL */
	long (*set_get_int)(char *); /* retrieves SET_INT */
	float (*set_get_float)(char *); /* retrieves SET_FLOAT */
	char *(*set_get_string)(char *); /* retrieves SET_*STRING */
	void (*set_remove)(SETTING *);
	void (*set_destroy)();

	PLUGIN *(*plug_load)(char *, OUTPUT *);
	int (*plug_unload)(PLUGIN *);
	void (*plug_add)(PLUGIN *);
	void (*plug_remove)(PLUGIN *);
	void (*plug_destroy)(void *);

	TRIGGER *(*plug_trigger_reg)(char *, u_int, e_trigger_f);
	int (*plug_trigger_unreg)(TRIGGER *);
	void (*plug_trigger_destroy)(void *);

	TIMER *(*plug_timer_reg)(u_long, e_timer_f, void *);
	int (*plug_timer_unreg)(TIMER *timer);
	int (*plug_timers)(void);
	void (*plug_timer_destroy)(void *data);

	/* output functions */
	pfn_output_open		*output_open;
	pfn_output_tostring     *output_tostring;
	pfn_output_print	*output_print;
	pfn_output_printf	*output_printf;
	pfn_output_close	*output_close;
	pfn_output_destroy	*output_destroy;

	/* functions from users.h */
	pfn_user_add      	*user_add;
	pfn_user_nickset  	*user_nickset;
	pfn_user_remove   	*user_remove;
	pfn_user_destroy  	*user_destroy;
	pfn_user_find     	*user_find;

	/* functions from connections.h */
	pfn_conn_http_get	*conn_http_get;
	pfn_conn_unreg		*conn_unreg;
	pfn_conn_close		*conn_close;
	pfn_conn_destroy	*conn_destroy;
	pfn_conn_connect	*conn_connect;
	pfn_conn_listen		*conn_listen;
	pfn_conn_accept		*conn_accept;	
	
	/* functions from servers.h */
	pfn_server_add		*server_add;
	pfn_server_remove	*server_remove;
	pfn_server_destroy	*server_destroy;
	

	
	CHANNEL *(*channel_add)(char *);
	void (*channel_remove)(CHANNEL *);
	void (*channel_update)();
	void (*channel_join)(void *);

	CHANNEL *(*channel_find)(char *);
	void (*channel_destroy)(void *);

	int (*plug_sendf)(char *fmt, ...);
	int (*plug_send)(char *);

	void (*log)(int, char *, ...);
	void (*plog)(PLUGIN *, int, char *, ...);

	int (*findip)(char *, struct in_addr *);
#ifdef IPV6
	int (*findip6)(char *, struct in6_addr *);	
#endif
};


#ifndef EIWIC_MODULE

/*
 *  Output functions
 */
pfn_output_open		output_open;
pfn_output_tostring 	output_tostring;
pfn_output_print	output_print;
pfn_output_printf	output_printf;
pfn_output_close	output_close;
pfn_output_destroy	output_destroy;

/*
 *  Setting functions
 */
SETTING* set_add(int, char *, ...);
SETTING* set_find(char *);
void set_set(SETTING *, ...);
int set_is(char *); /* retrieves SET_BOOL */
long set_get_int(char *); /* retrieves SET_INT */
float set_get_float(char *); /* retrieves SET_FLOAT */
char *set_get_string(char *); /* retrieves SET_*STRING */
void set_remove(SETTING *);
void set_destroy();
		
/*
 *	Plugin functions
 */
PLUGIN *plug_load(char *, OUTPUT *);
int plug_unload(PLUGIN *);
void plug_add(PLUGIN *);
void plug_remove(PLUGIN *);
void plug_destroy(void *);

/*
 *	Trigger functions
 */	
TRIGGER *plug_trigger_reg(char *, u_int, e_trigger_f);
int plug_trigger_unreg(TRIGGER *);
void plug_trigger_destroy(void *);

/*
 *	Timer functions
 */
TIMER *plug_timer_reg(u_long, e_timer_f, void *);
int plug_timer_unreg(TIMER *timer);
int plug_timers(void);
void plug_timer_destroy(void *data);

/*
 *	Channel functions
 */
CHANNEL *channel_add(char *);
void channel_remove(CHANNEL *);
void channel_update();
void channel_join(void *);

CHANNEL *channel_find(char *);
void channel_destroy(void *);

/*
 *	Misc functions
 */
int plug_sendf(char *fmt, ...);
int plug_send(char *);


void log(int, char *, ...);
void plog(PLUGIN *, int, char *, ...);

int findip(char *, struct in_addr *);
#ifdef IPV6
int findip6(char *, struct in6_addr *);	
#endif
void strtolower(char *);
void strtoupper(char *);

#ifdef IPV6
char *inet6_ntoa(struct in6_addr in6);
#endif

#endif

#ifdef EIWIC_MODULE
#ifndef _EIWIC_MODULE_H
#define _EIWIC_MODULE_H

GLOBAL *e_global; /* eiwic global settings and variables*/
MODLINK *mlink, *eiwic;	/* eiwic function pointers */

#define EP_SETNAME(name) char *ep_getname() { return (name); };
int ep_init(GLOBAL *g, PLUGIN *p, OUTPUT *o) {
	
	e_global = g;	
	mlink = eiwic = (MODLINK *)p->modlink;
#ifdef VV_DEBUG
	mlink->log(LOG_VERBOSE_DEBUG, "ep_main = %i", (u_long)p->ep_main);
#endif
	return (p->ep_main?p->ep_main(o):1); }
#endif
#endif
