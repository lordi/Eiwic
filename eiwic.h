/******************************************************************************
 * eiwic - Extensible Ircbot Written In C
 * Copyright (C) Hannes Gr�uler <lordi@styleliga.org>
 *
 * eiwic.h: Main header file.
 ****************************************************************************/

#ifndef _EIWIC_H_INCLUDED
#define _EIWIC_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <setjmp.h>

typedef unsigned char *STRING;

#ifndef NAME
#define NAME     "eiwic"
#endif
#ifndef VERSION
#define VERSION  "1.1.3"
#endif
#define AUTHOR   "Hannes Gr�uler <lordi@styleliga.org>"
#define EIWIC_HOMEPAGE "http://lordi.styleliga.org/eiwic/"

#define MSG      struct eiwic_ircmsg
#define PLUGIN   struct eiwic_plugin
#define TRIGGER  struct eiwic_plugin_trigger
#define TIMER    struct eiwic_plugin_timer
#define GLOBAL   struct eiwic_glob
#define CHANNEL  struct eiwic_channel
#define SETTING  struct eiwic_setting
#define MODLINK  struct eiwic_modlink
#define OUTPUT   struct eiwic_output

enum _LOG_VERBOSE_LEVELS {
	LOG_ERROR = 0,
	LOG_WARNING,
	LOG_STATUS,
	LOG_DEBUG,
	LOG_INIT_DEBUG,
#ifdef VV_DEBUG
	LOG_VERBOSE_DEBUG,
#else
	LOG_VERBOSE_DEBUG_DONT_USE,
#endif
	LOG_MISC,
	LOG_PLUGIN_OUTPUT
};

enum _TRIGGER_FLAGS {
	TRIG_PUBLIC = 0x02,
	TRIG_PRIVATE = 0x04,
	TRIG_ADMIN = 0x08
};

enum _SETTING_TYPES {
	SET_BOOL = 0,
	SET_INT,
	SET_FLOAT,
	SET_STRING,		/* 128 chars max */
	SET_LONGSTRING	/* unlimited length (be careful) */
	
};

enum _OUTPUT_TYPES {
	OP_EAT = 0,
	OP_LOG,
	OP_QUERY,
	OP_NOTICE,
	OP_CHANNEL,
	OP_SERVER,
	OP_SERVER_INPUT,
	OP_FILE,
	OP_FORWARD,
	OP_DOUBLE
};

enum _OUTPUT_BUFFER_TYPES {
	OB_NONE = 0,
	OB_LINE,
	OB_SERVER
};

#define SEGWARN(module,routine) \
	log(LOG_WARNING, "Segmentation fault in module %s.%s()",\
		(module)->name, routine);
#define MIN(x1,x2) \
	(((x1)<(x2))?(x1):(x2))

GLOBAL;
MSG;
TRIGGER;
OUTPUT;



#include "dlist.h"
#include "users.h"
#include "servers.h"



void irc_parse(struct eiwic_ircmsg *);
void irc_resolve(char *, struct eiwic_ircmsg *);

typedef int   (*e_trigger_f)(char *, OUTPUT *, MSG *);
typedef void  (*e_timer_f)(void *);

CHANNEL {
	u_char name[128];
	int joined;
	OUTPUT *output;
};

MSG {
	u_short cmd_num;
	u_char is_server;
	u_char cmd[10], to[50], args[512];
	u_char nick[30], username[30], host[512];
	u_char server[64];
	CHANNEL *channel;
	USER *user;
};


PLUGIN {
	u_char filename[255];
	u_char name[20];
	char inited;
	void *handle;
	void *modlink;
	OUTPUT *init_out;
	
	int (*ep_init)(GLOBAL *, PLUGIN *, OUTPUT *);
	int (*ep_main)(OUTPUT *);
	int (*ep_unload)(OUTPUT *);
	int (*ep_parse)(MSG *);
	int (*ep_trigger)(TRIGGER *, OUTPUT *, MSG *);
	int (*ep_help)(OUTPUT *);
};

TRIGGER {
	u_char trigger[30];
	e_trigger_f func;
	u_int flags;
	PLUGIN *plugin;
};

TIMER {
	u_long time;
	e_timer_f func;
	void *data;
	PLUGIN *plugin;
};

SETTING {
	char label[64];
	int type;
	union {
		char *d_string;
		float d_float;
		long d_int;
	} data;
};

OUTPUT {
	u_int op_id;
	u_int op_type;
	u_int op_buffered;
	union {
		USER *d_user; /* output to a user (query) */
		CHANNEL *d_channel; /* output to a channel */
		OUTPUT *d_output; /* output forward to another output */
		SERVER *d_server; /* output to a server */
		struct {
			OUTPUT *f1,*f2;
		} d_double;
		int d_loglevel; /* OP_LOG */
	} data;
	u_char *op_buffer;
};

GLOBAL {
	u_int	var_socket;
	u_char	quit, logged_in;
	u_char	botnick[30];
	
	u_char	vvd_section[40];
	u_char	*vvd_param;
	/*u_char	*b_send;*/

	u_char  mod_need_init;
	sigjmp_buf jmp_past;
	u_char jump_back;

	int		time_launched;

	int	var_bytes;
	int var_highest_output;

	int var_daemon;
	int var_log;
	int var_verbose;
	int var_noconnect;
	
	u_char var_logfilename[128];
	u_char var_testtrigger[300];

	FILE *var_logfile;

	void *modlink;

	OUTPUT *server_output, *log_output, *server_input, *plugin_output;

	SERVER *server, *proxy;

	DList
		plugins,
		triggers,
		timers,
		connections,
		channels,
		servers,
		users,
		settings,
		outputs;

};

extern GLOBAL *e_global;
extern PLUGIN *plugin;

#endif /* _EIWIC_H_INCLUDED */
