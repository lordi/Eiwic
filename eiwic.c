/******************************************************************************
 * eiwic - Extensible Ircbot Written In C
 * Copyright (C) Hannes Gr�uler <lordi@styleliga.org>
 *
 * eiwic.c: Main eiwic source file - IRC, connection and plugin handling
 *****************************************************************************/
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netdb.h>
#include <time.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>

#include "eiwic.h"
#include "settings.h"
#include "numerics.h"
#include "connections.h"
#include "plugins.h"

#define ARGS "l:v:t:dhn"

extern char *optarg;
extern int optind;
extern int opterr;
extern int optopt;

GLOBAL *e_global;
PLUGIN *plugin;

void sigint();
void sigsegv();

void enter_vvd_section(u_char *);

void irc_login();

void banner(void);
void syntax(char *);
int init(char *);
int launch(void);

int main(int argc, char *argv[])
{
	int c,
		verbose = 2,
		logging = 0,
		daemon = 0,
		noconnect = 0;
	char logfile[128], testtrigger[300];

	optarg = NULL;
	plugin = NULL;
	*logfile = 0;

	*testtrigger = '\0';
	
	banner();
	
	while ((c = getopt(argc, argv, ARGS)) != -1)
	{
		switch (c)
		{
		case 'h':
				syntax(argv[0]);
				exit(1);
			break;
		case 'n':
				noconnect = 1;
			break;
		case 't':
				noconnect = 1;
				strncpy(testtrigger, optarg, 299);
			break;
		case 'd':
				daemon = 1;
			break;
		case 'l':
				logging = 1;
				strncpy(logfile, optarg, 127);
			break;
		case 'v':
				verbose = atoi(optarg);
			break;
		default:
				syntax(argv[0]);
				exit(1);
			break;
		}
	}
	
	if (argv[optind] == NULL)
	{
		syntax(argv[0]);
		exit(1);
	}

	e_global = malloc(sizeof(struct eiwic_glob));
	bzero(e_global, sizeof(struct eiwic_glob));
		
	strcpy(e_global->var_logfilename, logfile);
	strcpy(e_global->var_testtrigger, testtrigger);

	e_global->var_daemon = daemon;
	e_global->var_log = logging;
	e_global->var_verbose = verbose;
	e_global->var_noconnect = noconnect;
	
	if (init(argv[optind]) >= 0)
	{
		log(LOG_STATUS, "Eiwic is launching.. (%i modules)", e_global->plugins.size);
		launch();
	}
	
	return 0;
}

#ifdef VV_DEBUG
void enter_vvd_section(u_char *sec)
{
	if (sec)
	{
		strncpy(e_global->vvd_section, sec, 40);
	}
	else
	{
		*e_global->vvd_section = 0;
		e_global->vvd_param = NULL;
	}	
}
#endif

int sigrised()
{
	e_global->jump_back = 1;
	return sigsetjmp(e_global->jmp_past, 1);
}

void sigfall()
{
	e_global->jump_back = 0;
}

void test_send_ping(void *t)
{
	plug_sendf("PING :%d\r\n", time(NULL));
	plug_timer_reg(time(NULL) + 10*60, test_send_ping, NULL);
}

/* from http://www.codeproject.com */
int wildcmp(char *wild, char *string) { 
	char *cp, *mp; 

	while ((*string) && (*wild != '*')) { 
		if ((*wild != *string) && (*wild != '?')) { 
			return 0; 
		} 
		wild++; 
		string++; 
	} 

	while (*string) { 
		if (*wild == '*') { 
		if (!*++wild) { 
		return 1; 
	} 
	mp = wild; 
	cp = string+1; 
	} else if ((*wild == *string) || (*wild == '?')) { 
		wild++; 
		string++; 
	} else { 
		wild = mp; 
		string = cp++; 
	} 
	} 

	while (*wild == '*') { 
	wild++; 
	} 
	return !*wild; 
}

void eiwic_trigger_parse(char *line, OUTPUT *out, MSG *ircmsg)
{
	char *trigger_str, *trigger_arg;
	PLUGIN *plugin, *p;
	TRIGGER *trigger;
	DListElmt *el;
	OUTPUT *put = out;

	trigger_str = strtok(line, " ");

	if (trigger_str != NULL)
	{
		trigger_arg = strtok(NULL, "");

		FOR_EACH(&e_global->triggers, el)
		{
		/*	log(LOG_VERBOSE_DEBUG, "Bla %s", trigger_arg); */

			trigger = el->data;
			if (strcasecmp(trigger->trigger, trigger_str) == 0
					&& trigger->func != NULL)
				{
					log(LOG_DEBUG, "Recognized trigger %s.", trigger->trigger);
					
					if (trigger->flags == TRIG_ADMIN && ircmsg != NULL)
					{
						char smask[600];
						sprintf(smask, "%s!%s@%s",	ircmsg->nick, ircmsg->username, ircmsg->host);
						
						log(LOG_DEBUG, 
							"%s: is an ADMIN trigger, checking %s against %s.", trigger->trigger,
							set_get_string("admin_mask"), smask);

						if (!wildcmp(set_get_string("admin_mask"), smask)) continue;
					}

					if (put == NULL && ircmsg != NULL)
					{
						if (ircmsg->channel)
						{
							log(LOG_DEBUG, "output will be CHANNEL '%s'", ircmsg->channel->name);

							put = ircmsg->channel->output;
						}
						else if (ircmsg->user)
						{
							if (strcmp(ircmsg->cmd, "notice") == 0) {
								log(LOG_DEBUG, "output will be USER '%s' [notice]", ircmsg->user->nick);
								if (ircmsg->user->output_notice == NULL)
								{
									ircmsg->user->output_notice = output_open(OP_NOTICE, ircmsg->user);
								}
								put = ircmsg->user->output_notice;
							}
							else {
								log(LOG_DEBUG, "output will be USER '%s' [query]", ircmsg->user->nick);
								if (ircmsg->user->output == NULL)
								{
									ircmsg->user->output = output_open(OP_QUERY, ircmsg->user);
								}
								put = ircmsg->user->output;
							}
						}
					}
					else if (put != NULL)
					{
						log(LOG_DEBUG, "output will be %d", put->op_id);
					}

					log(LOG_DEBUG, "%s: calling "
							"ep_triggers and "
							"function from module %s",
							trigger->trigger,
							(trigger->plugin?
							 trigger->plugin->name:
							 NULL));
					
					FOR_EACH_DATA(&e_global->plugins, p)								
						if (p->ep_trigger != NULL)		
						{
							if (sigrised() == 0)
							{
								plugin = p;
								p->ep_trigger(trigger, put, ircmsg);
								plugin = NULL;
							}
							else
							{
								sigfall();
								SEGWARN(p, "ep_trigger");
							}
							sigfall();
						}
					END_DATA
			
					if (sigrised() == 0)
					{
						plugin = trigger->plugin;
						trigger->func(trigger_arg, put, ircmsg);
						plugin = NULL;
					}
					else
					{
						sigfall();
						SEGWARN(trigger->plugin, "e_trigger_f");
					}
					sigfall();

#ifdef VV_DEBUG
					log(LOG_VERBOSE_DEBUG, "ep_trigger and module func finished.");
#endif
			}
		}	
	}

/*	log(LOG_VERBOSE_DEBUG, "Bla2"); */
}

void eiwic_trigger_parse_this(char *t) {
	eiwic_trigger_parse(t, e_global->plugin_output, NULL);
}


void irc_parse(struct eiwic_ircmsg *ircmsg)
{
	volatile DListElmt *el;
	
	PLUGIN *plug, *p;
	TRIGGER *trigger;	
	MSG m;
	char *trigger_str, *trigger_arg;

	memcpy(&m, ircmsg, sizeof(MSG));
		
#ifdef VV_DEBUG

	enter_vvd_section("irc_parse");
	
	if (ircmsg->is_server)
	{
		if (ircmsg->cmd_num > 0)
			log(LOG_DEBUG, "[IRCMSG] (server) %s sends %i to %s: %s",
				ircmsg->server, ircmsg->cmd_num, ircmsg->to, ircmsg->args);
		else
			log(LOG_DEBUG, "[IRCMSG] (server) %s sends %s to %s: %s",
				ircmsg->server, ircmsg->cmd, ircmsg->to, ircmsg->args);
	}
	else
	{
		if (ircmsg->cmd_num > 0)
			log(LOG_DEBUG, "[IRCMSG] %s (%s@%s) sends %i to %s: %s",
				ircmsg->nick, ircmsg->username, ircmsg->host, ircmsg->cmd_num, ircmsg->to, ircmsg->args);
		else
			log(LOG_DEBUG, "[IRCMSG] %s (%s@%s) sends %s to %s: %s",
				ircmsg->nick, ircmsg->username, ircmsg->host, ircmsg->cmd, ircmsg->to, ircmsg->args);
	}
#endif

	if (ircmsg->cmd_num == 0)
	{
		/* TODO: "nick" -> user_nickset */
		if (strcmp(ircmsg->cmd, "ping") == 0 && ircmsg->is_server) {
			plug_sendf("PONG :%s\r\n", ircmsg->args);
		}
		else
		if (strcmp(ircmsg->cmd, "error") == 0) {
			log(LOG_WARNING, "Error from server: %s", ircmsg->args);
		}
		else
		if (strcmp(ircmsg->cmd, "join") == 0) {
			if (strcmp(ircmsg->nick, e_global->botnick) == 0)
			{
				CHANNEL *chan = channel_add(ircmsg->args);
				chan->joined = 1;
			}
			user_add(ircmsg->nick);
		}
		else
		if (strcmp(ircmsg->cmd, "kick") == 0) {
			if (strcasecmp(strtok(ircmsg->args, " "), e_global->botnick) == 0)
			{
				log(LOG_DEBUG, "I was kicked from %s by %s! I hate this mofo!",
						ircmsg->to, ircmsg->nick, ircmsg->args);
				plug_timer_reg(time(NULL) + set_get_int("rejoin_sleep"), channel_join,
						channel_find(ircmsg->to));
			}
		}
		else
		if (strcmp(ircmsg->cmd, "privmsg") == 0) {
			memcpy(&m, ircmsg, sizeof(struct eiwic_ircmsg));
			eiwic_trigger_parse((char *)m.args, NULL, &m);
		}
		else
		if (strcmp(ircmsg->cmd, "notice") == 0) {
			memcpy(&m, ircmsg, sizeof(struct eiwic_ircmsg));
			eiwic_trigger_parse((char *)m.args, NULL, &m);
		}
	}
	else
	switch (ircmsg->cmd_num)
	{
	case RPL_WELCOME:
			strcpy(e_global->botnick, ircmsg->to);
		
			log(LOG_STATUS, "Successfully logged into IRC server");
#ifdef VV_DEBUG
			log(LOG_VERBOSE_DEBUG, "----> Server=%s; Nickname=%s", ircmsg->server, e_global->botnick);
#endif

			e_global->logged_in = 1;

			plug_timer_reg(time(NULL) + 100, test_send_ping, NULL);

			/* Join all channels */
			channel_update();
		break;
	case RPL_NAMREPLY:
			{
				char *chan, *users, *nick;
				
				if ((chan = strtok(ircmsg->args, ":")) == NULL)
					break;
				
				if (*chan == '@' || *chan == '+')
					chan += 2;
					
				if ((users = strtok(NULL, "")) == NULL)
					break;
				
				nick = strtok(users, " ");

				if (nick) do
				{
					if (*nick == '@' ||
			          		*nick == '+' ||
						*nick == '%' ||
						*nick == '&')
						nick++;
						
					user_add(nick);
				} while (nick = strtok(NULL, " "));
				
			}
		break;
	case ERR_NICKNAMEINUSE:
			if (e_global->logged_in == 0)
			{
				/* this sucks but anyway */
				char buf[17];
				strncpy(buf, set_get_string("nick"), 15);
				sprintf(&buf[strlen(buf)>7?7:strlen(buf)], "%i", rand() % 99);
				plug_sendf("NICK %s\r\n", buf);
			}
		break;
	}

	init_modules();

	FOR_EACH(&e_global->plugins, el)
	{
		plug = el->data;
		
		if (plug->ep_parse == NULL)
			continue;
		
		if (sigrised() == 0)		
		{
			plugin = plug;
			plugin->ep_parse(ircmsg);
			plugin = NULL;
		}
		else
		{
			sigfall();			
			SEGWARN(plugin, "ep_parse");			
		}
		
		sigfall();
	}

	/* Post-module-processing parsing... */

	if (ircmsg->cmd_num == 0)
	{
		if (strcmp(ircmsg->cmd, "quit") == 0) {
			user_remove(ircmsg->user);
		}
		else
		if (strcmp(ircmsg->cmd, "nick") == 0) {
			if (strcmp(ircmsg->user->nick, e_global->botnick) == 0)
			{
				strcpy(e_global->botnick, ircmsg->args);
			}
		}
		else
		if (strcmp(ircmsg->cmd, "part") == 0) {
			if (strcmp(ircmsg->nick, e_global->botnick) == 0
				&& ircmsg->channel != NULL)
			{
				ircmsg->channel->joined = 0;
			}
		}
	}
}

void irc_resolve(char *buf, struct eiwic_ircmsg *ircmsg)
{
	char *loc,
	  	*from = NULL,
	  	*to =NULL,
	  	*command=NULL,
	  	*args=NULL,
	  	from_nick[40],
	  	from_user[20],
	  	from_host[255],
	  	*edoff;
  	
#ifdef VV_DEBUG
	enter_vvd_section("irc_resolve");
	e_global->vvd_param = buf;
#endif
	
	bzero(ircmsg, sizeof(MSG));
	bzero(from_nick, 40);
	bzero(from_user, 20);
	bzero(from_host, 255);
		
	loc = (char *)malloc(strlen(buf) + 1);
			
	strcpy(loc, buf);
	
	if (loc[0] == ':')
	{
#ifdef VV_DEBUG
		enter_vvd_section("irc_resolve:chk1");
#endif
		
		from = strtok(loc, " ") + 1;
		command = strtok(NULL, " ");
		to = strtok(NULL, "\r\n");

		if (*to == ':')
		{
			args = to + 1;
			to = NULL;
		}
		else
		{
			if ((args = strchr(to, ' ')) != NULL)
			{
				*args = 0;
				args++;
				if (*args == ':') args++;
			}
			else args = "";
		}

#ifdef VV_DEBUG
		enter_vvd_section("irc_resolve:chk2");
#endif

		strncpy(from_nick, from, sizeof(from_nick)-1);
			
		edoff = strchr(from_nick, '!');
		if (edoff)
		{
		      *edoff=0;
		
		      strncpy(from_user, strchr(from, '!') + 1,
					sizeof(from_user)-1);
			  
		      edoff = strchr(from_user, '@');
		      *edoff=0;
		
		      strcpy(from_host, strchr(from, '@') + 1);
		}
	}
	else
	{
		args = strchr(loc, ' ') + 1;
		if (args)
		{
			if (args[0] == ':') args++;		
			command = strtok(loc, " ");
			strtok(args, "\r\n");
		}
		else
		{
			command = loc;
		}
	}

#ifdef VV_DEBUG
	enter_vvd_section("irc_resolve:chk3");
#endif
	
	if (command) ircmsg->cmd_num = atoi(command);
	if (command) strcpy(ircmsg->cmd, command);
	if (to)
	{
		CHANNEL *c;

		strcpy(ircmsg->to, to);

		if (c = channel_find(ircmsg->to))
		{
			ircmsg->channel = c;
		}
	}
	if (args) strcpy(ircmsg->args, args);
		
	if (strlen(from_user) == 0)
	{
		ircmsg->is_server = 1;
		strcpy(ircmsg->server, from_nick);
	}
	else
	{
		ircmsg->is_server = 0;
		strcpy(ircmsg->nick, from_nick);
		strcpy(ircmsg->username, from_user);
		strcpy(ircmsg->host, from_host);
		ircmsg->user = user_add(ircmsg->nick);
	}
	
	strtolower(ircmsg->cmd);
	free(loc);
}

void eiwic_test_trigger()
{
}

int proxy_login()
{
	char buf[2048], *s;
	int len, x;
	
	sprintf(buf, "CONNECT %s:%i HTTP/1.0\r\n\r\n",
			e_global->server->host, e_global->server->port);

	send(e_global->var_socket, buf, strlen(buf), 0);
	
	*buf = 0;
	x = 0;

	while ((len = recv(e_global->var_socket, &buf[x], 512, 0)) > 0)
	{
		buf[x + len] = 0;

		if (strstr(buf, "\r\n\r\n"))
		{			
#ifdef VV_DEBUG
			log(LOG_DEBUG, "%s", buf);
#endif

			if (strtok(buf, " "))
			{
				s = strtok(NULL, " ");
				if (s)
					if (atoi(s) == 200)
					{
						return 1;
					}
			}

			close(e_global->var_socket);
			return -1;
		}

		x = strlen(buf);
	}
	
	close(e_global->var_socket);
	return -1;
}

void irc_login()
{
	plug_sendf("USER %s eiwic eiwic :%s\r\n",
			set_get_string("username"),
			set_get_string("realname"));
	plug_sendf("NICK %s\r\n", set_get_string("nick"));
}

void eiwic_exit(int i) /* exit() wrapper */
{
	CONN *conn;
	
	log(LOG_STATUS, "Shutting down sockets and closing connections..", i);
	
del_conn:
	FOR_EACH_DATA(&e_global->connections, conn)
	{
		conn_close(conn);
		goto del_conn;
	}
	END_DATA;
	
	log(LOG_STATUS, "Eiwic quitting (%d)..", i);
	exit(i);
}

void sigsegv()
{
	if (e_global->jump_back == 1)
	{
		plugin = NULL;
		siglongjmp(e_global->jmp_past, 0);
	}
	
	log(LOG_ERROR, "Segmentation fault. Quitting.");
#ifdef VV_DEBUG
	log(LOG_DEBUG, "Segfault in vvd_section: %s", e_global->vvd_section);
	if (e_global->vvd_param)
		log(LOG_DEBUG, "Segfault w/ vvv_param: %s", e_global->vvd_param);
#endif

	eiwic_exit(1);
}

void sigint()
{
	/* By pressing Ctrl-C one time, the bot will try
		to disconnect from the server and quit. If the
		disconnect doesn't succeed, another Ctrl-C will
		make the program exit directly.
	*/
	
	if (e_global->quit == 1)
	{
		eiwic_exit(1);
	}
	else
	{
		e_global->quit = 1;
	
		plug_send("QUIT :SIGINT\r\n");

		plug_send(NULL);
	}
}

void banner(void)
{
}

void syntax(char *argv0)
{
	fprintf(stdout, "usage: %s [options] <configfile>\n", argv0);
	fprintf(stdout, "options:\n");
	fprintf(stdout, "  -h         show help\n");
	fprintf(stdout, "  -t '!trig' use this for testing triggers offline\n");
	fprintf(stdout, "  -d         daemon mode, fork into background\n");
	fprintf(stdout, "  -l file    message log into a file (if file is \"-\", it will be\n");
	fprintf(stdout, "             printed to stdout)\n");
	fprintf(stdout, "  -v level   verbosity level to log messages, default = 1\n");
	fprintf(stdout, "             0 = only log errors\n");
	fprintf(stdout, "             1 = also log warnings\n");
	fprintf(stdout, "             2 = also log normal status messages\n");
	fprintf(stdout, "             3 = also log debug messages\n");
	fprintf(stdout, "             4 = also log debug messages for initialization\n");
	fprintf(stdout, "             5 = also log very verbose debug messages\n");
	fprintf(stdout, "                 ('./configure --vv-debug' required)\n\n");
	fprintf(stdout, "examples:\n");
	fprintf(stdout, "1) Interesting output to stdout:\n");
	fprintf(stdout, "   %s -l - -v 2 bot.conf\n", argv0);
	fprintf(stdout, "2) Background mode with logfile:\n");
	fprintf(stdout, "   %s -l eiwic_error.log -d bot.conf\n", argv0);
	fprintf(stdout, "3) Debug bot:\n");
	fprintf(stdout, "   %s -l - -v 10 bot.conf\n", argv0);
	fprintf(stdout, "4) Debug your trigger without connecting to a server:\n");
	fprintf(stdout, "   %s -l - -v 3 bot.conf -t '!say rockn roll'\n", argv0);
	fprintf(stdout, "5) Spit out nothing else then the trigger output:\n");
	fprintf(stdout, "   %s -l - -v 0 bot.conf -t '!say rockn roll'\n", argv0);
	fflush(stdout);
}

int init_modules()
{
	int err = 0;
	PLUGIN *p;
	DListElmt *el;

	if (e_global->mod_need_init == 1)
	{
		log(LOG_STATUS, "Initializing modules..");
		
		FOR_EACH(&e_global->plugins, el)
		{
			p = el->data;
			
			if (p->ep_init && p->inited == 0)
			{
				e_global->jump_back = 1;
				if (sigsetjmp(e_global->jmp_past, 1) == 0)
				{
#ifdef VV_DEBUG
					log(LOG_VERBOSE_DEBUG, "module(%s) %i,%i", p->name, p, p->ep_init);
#endif

					plugin = p;
					
					if (p->ep_init(e_global, p, p->init_out) == 0)
					{
						log(LOG_ERROR, "Initialization of %s failed.", p->name);
						err++;
						plugin = NULL;
						/* todo: unload modul */
						break;
					}

					p->inited = 1;
					plugin = NULL;
				}
				else
				{
					e_global->jump_back = 0;
					SEGWARN(p, "ep_init");
					/* todo: unload modul */
				}
				e_global->jump_back = 0;
			}
		}
	}

	e_global->mod_need_init = 0;
}

int init(char *conf)
{
	int err = 0;
	PLUGIN *p;
	DListElmt *el;
	MODLINK *link;


#ifdef VV_DEBUG
	enter_vvd_section("init");
#endif
	
	srand(time(NULL));
		
	signal(SIGHUP,SIG_IGN);
	signal(SIGPIPE,SIG_IGN);
	signal(SIGINT, sigint);
	signal(SIGSEGV, sigsegv);
	
	if (e_global->var_log)
	{
		if (strcmp(e_global->var_logfilename, "-") == 0)
		{
			strcpy(e_global->var_logfilename, "stdout");
			e_global->var_logfile = stdout;
		}
		else
		{
			e_global->var_logfile = fopen(e_global->var_logfilename, "a");
		}
			

		if (e_global->var_logfile == NULL)
		{
			err++;
			perror("Unable to open log file");
			goto abort;
		}
	}
	
	log(LOG_STATUS, "Initializing eiwic..");
	
	dlist_init(&e_global->plugins, plug_destroy);
	dlist_init(&e_global->triggers, plug_trigger_destroy);
	dlist_init(&e_global->timers, plug_timer_destroy);
	dlist_init(&e_global->connections, conn_destroy);
	dlist_init(&e_global->channels, channel_destroy);
	dlist_init(&e_global->servers, server_destroy);
	dlist_init(&e_global->settings, set_destroy);
	dlist_init(&e_global->users, user_destroy);
	dlist_init(&e_global->outputs, output_destroy);

	e_global->log_output = output_open(OP_LOG, LOG_STATUS);

	link = malloc(sizeof(MODLINK));

	link->set_add = set_add;
	link->set_find = set_find;
	link->set_set = set_set;
	link->set_is = set_is;
	link->set_get_int = set_get_int;
	link->set_get_float = set_get_float;
	link->set_get_string = set_get_string;
	link->set_remove = set_remove;

	link->plug_load = plug_load;
	link->plug_add = plug_add;
	link->plug_unload = plug_unload;
	link->plug_remove = plug_remove;
	link->plug_destroy = plug_destroy;
	link->plug_trigger_reg = plug_trigger_reg;
	link->plug_trigger_destroy = plug_trigger_destroy;
	link->plug_trigger_unreg = plug_trigger_unreg;
	link->plug_timer_reg = plug_timer_reg;
	link->plug_timer_destroy = plug_timer_destroy;
	link->plug_timer_unreg = plug_timer_unreg;
	link->plug_timers = plug_timers;
	
	link->conn_http_get = 	conn_http_get;	
	link->conn_unreg = 	conn_unreg;
	link->conn_connect = 	conn_connect;
	link->conn_destroy = 	conn_destroy;
	link->conn_close = 	conn_close;
	link->conn_listen =	conn_listen;
	link->conn_accept =	conn_accept;

	link->server_add = 	server_add;
	link->server_destroy = 	server_destroy;
	link->server_remove = 	server_remove;

	link->channel_add = channel_add;
	link->channel_remove = channel_remove;
	link->channel_destroy = channel_destroy;
	link->channel_join = channel_join;
	link->channel_find = channel_find;

	link->user_add      	=user_add;
	link->user_nickset  	=user_nickset;
	link->user_remove   	=user_remove;
	link->user_destroy  	=user_destroy;
	link->user_find     	=user_find;
	
	link->output_open = output_open;
	link->output_print	= output_print;
	link->output_tostring	= output_tostring;
	link->output_printf	= output_printf;
	link->output_close	= output_close;
	link->output_destroy	= output_destroy;
	
	link->plug_send = plug_send;
	link->plug_sendf = plug_sendf;

	link->log = log;
	link->plog = plog;

	link->findip = findip;
#ifdef IPV6
	link->findip6 = findip6;
#endif
	
	e_global->modlink = (void *)link;

	set_add(SET_STRING, "config_file", conf);
	set_add(SET_INT, "connect_sleep", 10);
	set_add(SET_INT, "exhaused_sleep", 40);
	set_add(SET_INT, "rejoin_sleep", 7);
	
	set_add(SET_STRING, "nick", "");
	set_add(SET_STRING, "username", "eiwic");
	set_add(SET_STRING, "realname", "the splitrider's bot");
		
	set_add(SET_STRING, "admin_mask", ""); /* no admin by default */

	set_add(SET_STRING, "vhost", "");
	set_add(SET_STRING, "proxy_host", "");
	set_add(SET_INT, "proxy_port", 3128);
	
	set_add(SET_STRING, "module_path", "./");
	
	if (conf_loadfile() == -1)
	{
		err++;
		goto abort;
	}

	init_modules();	

	if (set_is("proxy_host"))
	{
		e_global->proxy = malloc(sizeof(SERVER));
		bzero(e_global->proxy, sizeof(SERVER));

		strncpy(e_global->proxy->host, set_get_string("proxy_host"), 127);
		e_global->proxy->port = set_get_int("proxy_port");		
	}
	else
	{
		e_global->proxy = NULL;
	}

	e_global->server_input = output_open(OP_SERVER_INPUT);

	if (e_global->plugins.size <= 0)
	{
		log(LOG_WARNING, "No modules has been added!");
	}
	
	if (e_global->channels.size <= 0)
	{
		log(LOG_WARNING, "No channel has been added!");
	}

	if (e_global->servers.size <= 0)
	{
		err++;
		log(LOG_ERROR, "No server has been added!");
	}

abort:
	if (err > 0)
	{
		log(LOG_WARNING, "Initialization failed with %i error(s)", err);
		return -1;
	}
	
	return 0;
}
/*
int readconn(CONN *con, char *data, u_int len)
{
	data[len]=0;
	log(LOG_DEBUG, "incoming %s", data);
	send(con->sock, data, len, 0);
	return 0;
}

int newconn(CONN *con)
{
	log(LOG_DEBUG, "incoming");
	send(con->sock, "Hallo", 5, 0);
	return 0;
}
*/
int launch(void)
{
	SERVER *serv;
	
#ifdef VV_DEBUG
	enter_vvd_section("launch");
#endif
	
	if (e_global->var_daemon)
	{
		int pid;
		
		log(LOG_DEBUG, "Forking into background");
		if ((pid = fork()) > 0)
		{
			log(LOG_STATUS, "Forked into background (pid=%i)", pid);
			exit(1);
		}
		else if (pid != 0)
		{
			log(LOG_ERROR, "fork() failed");
			eiwic_exit(1);
		}
	}

	e_global->time_launched = time(NULL);
	e_global->server = e_global->servers.head->data;

	while (e_global->quit == 0)
	{	
		u_char buffer[800];
		MSG ircmsg;
		struct sockaddr *sa;
		int sa_len;
		
	/*	conn_listen(6666, 2, newconn, readconn, NULL, NULL); */

		if (e_global->var_noconnect == 0)
		{
			
#ifdef VV_DEBUG
		enter_vvd_section("connect");
#endif
	
		if (e_global->proxy)
		{
			serv = e_global->proxy;
		}
		else
		{
			serv = e_global->server;
		}
	
#ifdef IPV6
		if (serv->ipv6)
		{	
			e_global->var_socket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
		}
		else
#endif
		{
			e_global->var_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

		}
		
		if (!e_global->var_socket)
		{
			log(LOG_ERROR, "Couldn't create socket");
			return -1;
		}
		
		if (set_is("vhost"))
		{
			char *vhost = set_get_string("vhost");
			
			log(LOG_DEBUG, "Resolving virtual host '%s'..", vhost);

#ifdef IPV6
			if (serv->ipv6)
			{
				bzero(&serv->lsin6, sizeof(struct sockaddr_in6));
				serv->lsin6.sin6_family = AF_INET6;
				serv->lsin6.sin6_port = 0;
			
				if (findip6(vhost, &serv->lsin6.sin6_addr) == -1)
				{
					log(LOG_ERROR, "Unable to resolve IPv6 host \"%s\"", vhost);
					goto reconnect;
				}
			
				log(LOG_DEBUG, "IPv6 from \"%s\" = \"%s\"", vhost, inet6_ntoa(serv->lsin6.sin6_addr));
				
				if (bind(e_global->var_socket, (struct sockaddr *)&serv->lsin6, sizeof(struct sockaddr_in6)) == -1)
				{
					log(LOG_ERROR, "Unable to bind socket to vhost: \"%s\"", inet6_ntoa(serv->lsin6.sin6_addr));
					goto reconnect;
				}
			}
			else
#endif
			{
				bzero(&serv->lsin, sizeof(struct sockaddr_in));
				serv->lsin.sin_family = AF_INET;
				serv->lsin.sin_port = 0;
			
				if (findip(vhost, &serv->lsin.sin_addr) == -1)
				{
					log(LOG_ERROR, "Unable to resolve host \"%s\"", vhost);
					goto reconnect;
				}

				log(LOG_DEBUG, "IP from \"%s\" = \"%s\"", vhost, inet_ntoa(serv->lsin.sin_addr));
				
				if (bind(e_global->var_socket, (struct sockaddr *)&serv->lsin, sizeof(struct sockaddr_in)) == -1)
				{
					log(LOG_ERROR, "Unable to bind socket to vhost: \"%s\"", inet_ntoa(serv->lsin.sin_addr));
					goto reconnect;
				}
			}

		}

		log(LOG_DEBUG, "Resolving host '%s'..", serv->host);
		
#ifdef IPV6
		if (serv->ipv6)
		{
			bzero(&serv->sin6, sizeof(struct sockaddr_in6));
			serv->sin6.sin6_family = AF_INET6;
			serv->sin6.sin6_port = htons(serv->port);
			
			if (findip6(serv->host, &serv->sin6.sin6_addr) == -1)
			{
				log(LOG_ERROR, "Unable to resolve IPv6 host \"%s\"", serv->host);
				goto reconnect;
			}
			
			log(LOG_DEBUG, "IPv6 from \"%s\" = \"%s\"", serv->host, inet6_ntoa(serv->sin6.sin6_addr));
		}
		else
#endif
		{
			bzero(&serv->sin, sizeof(struct sockaddr_in));
			serv->sin.sin_family = AF_INET;
			serv->sin.sin_port = htons(serv->port);
			
			if (findip(serv->host, &serv->sin.sin_addr) == -1)
			{
				log(LOG_ERROR, "Unable to resolve host \"%s\"", serv->host);
				goto reconnect;
			}
			
			log(LOG_DEBUG, "IP from \"%s\" = \"%s\"", serv->host, inet_ntoa(serv->sin.sin_addr));
		}
		
		log(LOG_DEBUG, "Connecting to IRC server..");

#ifdef IPV6
		if (serv->ipv6)
		{
			sa = (struct sockaddr *)&serv->sin6;
			sa_len = sizeof(struct sockaddr_in6);
		}
		else
#endif
		{
			sa = (struct sockaddr *)&serv->sin;
			sa_len = sizeof(struct sockaddr_in);
		}
	
		if (connect(e_global->var_socket, sa, sa_len) == -1)
		{
			log(LOG_WARNING, "Unable to connect to IRC server: %s:%i",
					inet_ntoa(serv->sin.sin_addr), htons(serv->sin.sin_port));
			goto reconnect;
		}
		
		if (e_global->proxy)
		{
			if (proxy_login() < 0)
			{
				log(LOG_WARNING, "Attention: Proxy failes or denies connection!");
				goto reconnect;
			}
		}
		
		log(LOG_DEBUG, "Hey, we are connected");

		e_global->server_output = output_open(OP_SERVER, serv);

		irc_login();

		}
		else
		{
			log(LOG_STATUS, "eiwic running in no-connect mode.");

			e_global->server_output = output_open(OP_LOG, LOG_STATUS);

			if (strlen(e_global->var_testtrigger) > 0)
			{
				char *s = e_global->var_testtrigger;
				int i = 1;
				
				e_global->plugin_output = output_open(OP_LOG, LOG_PLUGIN_OUTPUT);
				
				log(LOG_STATUS, "eiwic is going to test-run trigger line(s): '%s'",
					e_global->var_testtrigger);	
				
				while (s) {	
					char *p = s;
					s = strchr(s, ';');
					if (s != NULL) {
						*s = '\0';
						s++; 	
					}		
					
					plug_timer_reg(time(NULL) + i, eiwic_trigger_parse_this, p);
					i += 2;
				}
			}
		}

		{
			struct timeval tv;

			CONN *con;
			TIMER *tim;
			DListElmt *el;
			
       		fd_set ircfd, writefd;
			u_long t, highest=0, sec;
			t = time(NULL);
			
			*buffer = 0;
			
			while (1)
			{
				FD_ZERO(&ircfd);
				FD_ZERO(&writefd);

				if (e_global->var_noconnect == 0)
				{
					FD_SET(highest = e_global->var_socket, &ircfd);
				}
				
				FOR_EACH(&e_global->connections, el)
				{
					con = el->data;
					
					if (con->status == CONN_CONNECTING)
					{
#ifdef VV_DEBUG
						log(LOG_VERBOSE_DEBUG, "%d", con->status);
#endif
						FD_SET(con->sock, &writefd);
					}
					else					
						FD_SET(con->sock, &ircfd);

					highest = (con->sock > highest) ? con->sock : highest;
				}

				sec = 50;
				
#ifdef VV_DEBUG
				log(LOG_VERBOSE_DEBUG, "Debug: %d timers", dlist_size(&e_global->timers));
#endif
				
				if (e_global->server_output->op_buffer == NULL)
				{
					FOR_EACH(&e_global->timers, el)
					{
						tim = el->data;
#ifdef VV_DEBUG
						log(LOG_VERBOSE_DEBUG, "Debug: p:%p %d diff:%d", tim, tim->time, tim->time - t);
#endif
						
						if (tim->time - t < sec)
							sec = tim->time - t;
					}
				}
				else sec = 1;
				
				
				tv.tv_sec = sec + 1;
				tv.tv_usec = 0;
#ifdef VV_DEBUG
				enter_vvd_section("select");
#endif

				if (select(highest + 1, &ircfd, &writefd, NULL, &tv) == -1)
				{
					log(LOG_ERROR, "select() failed");
					eiwic_exit(1);
				}

sel_check_conn:
				FOR_EACH(&e_global->connections, el)
				{
					con = el->data;
					
					if (FD_ISSET(con->sock, &writefd))
					{
						if (con->status == CONN_CONNECTING)
						{
							struct sockaddr_in sin;
							int l = sizeof(struct sockaddr);
								
							if (getpeername(con->sock, (struct sockaddr *)&sin, &l) == -1)
							{
								plog(con->plugin, LOG_WARNING, "Connection failed to %s:%i\n",
										inet_ntoa(*(struct in_addr*)&con->host), con->port);
								
							}
							
							con->status = CONN_ESTABLISHED;
							plog(con->plugin, LOG_DEBUG, "Connection established to %s:%i",
									inet_ntoa(*(struct in_addr*)&con->host), con->port);
								
							FD_CLR(con->sock, &writefd);
							if (con->cbe) 
							{
								e_global->jump_back = 1;
								if (sigsetjmp(e_global->jmp_past, 1) == 0)
								{
									plugin = con->plugin;
									con->cbe(con);
									plugin = NULL;
								}
								else
								{
									e_global->jump_back = 0;
									SEGWARN(con->plugin, "e_conn_established_f");
								}
								e_global->jump_back = 0;
								goto sel_check_conn;
							}
						}
					}
					
					if (FD_ISSET(con->sock, &ircfd))
					{
						int l = sizeof(struct sockaddr);
						char buffer[1028];
						
#ifdef VV_DEBUG
						plog(con->plugin, LOG_DEBUG, "select() fd set:%d %d %s %d",
									e_global->var_socket, con->sock,
									inet_ntoa(*(struct in_addr*)&con->host), con->port);
#endif
						
						switch (con->status)
						{
						case CONN_LISTENING:
							{
								CONN *acc;
								log(LOG_WARNING, "INCOMING CONNECTION.. HOORAY");
								
								acc = conn_accept(con);
								FD_CLR(con->sock, &ircfd);
								
								if (con->cbi)
								{
									e_global->jump_back = 1;
									if (sigsetjmp(e_global->jmp_past, 1) == 0)
									{
										plugin = con->plugin;
										con->cbi(acc);
										plugin = NULL;
									}
									else
									{
										e_global->jump_back = 0;
										SEGWARN(con->plugin, "e_conn_incoming_f");
									}
									e_global->jump_back = 0;
									
									goto sel_check_conn;
								}
							}
							break;
						case CONN_ESTABLISHED:
							{
								FD_CLR(con->sock, &ircfd);
								if ((l = recv(con->sock, buffer, 1027, 0)) > 0)
								{
									buffer[l] = 0;
									if (con->cbr)
									{
										e_global->jump_back = 1;
										if (sigsetjmp(e_global->jmp_past, 1) == 0)
										{
											plugin = con->plugin;
											con->cbr(con, buffer, l);
											plugin = NULL;
										}
										else
										{
											e_global->jump_back = 0;
											SEGWARN(con->plugin, "e_conn_read_f");
										}
										e_global->jump_back = 0;
									}
									goto sel_check_conn;
								}
								
								else if (l == 0 || (l == -1 && errno != EAGAIN))
								{
									FD_CLR(con->sock, &ircfd);
									conn_close(con);
									goto sel_check_conn;
								}
								/*
								else if (l == -1)
								{
									if (errno != EAGAIN)
									{
										plog(con->plugin, LOG_ERROR, "Connection closed: recv");
										conn_close(con);
									}
								}*/
							}
							break;
						}
					}
				}

				if (e_global->var_noconnect == 0)
				if (FD_ISSET(e_global->var_socket, &ircfd))
				{
					char xbuffer[50];
					int r;

					r = read(e_global->var_socket, xbuffer, sizeof(xbuffer) - 1);

					xbuffer[r] = 0;

					output_print(e_global->server_input, xbuffer);
					/*
					 
					if (r > 0)
					{
						buffer[r + l] = 0;
						data = buffer;
														 
						while ((end = strchr(data, '\n')) != NULL)
						{
							*end = 0;

							irc_resolve(data, &ircmsg);
							irc_parse(&ircmsg);
							
							data = end + 1;
							
							
						}
						 
						if (end == NULL && *data != 0)
						{							
							strcpy(buffer, data);
						}
						else
							*buffer = '\0';
					}
				*/
					 
					if (r == 0)
					{
						close(e_global->var_socket);
						log(LOG_ERROR, "Disconnected from IRC server");
						goto reconnect;
					}
						
					else if (r == -1)
					{
						if (errno != EAGAIN)
						{
							log(LOG_WARNING, "recv() failed");
							goto reconnect;
						}
					}
						
				}


#ifdef VV_DEBUG
				enter_vvd_section("init_modules");
#endif
				/* init modules that hasn't been inited yet */
				init_modules();
	 
#ifdef VV_DEBUG
				enter_vvd_section("plug_send");
#endif
				plug_send(NULL);

#ifdef VV_DEBUG
				enter_vvd_section("plug_timers");
#endif
				 
				plug_timers();
				 
				if (t < time(NULL))
				{
					e_global->var_bytes -= 110;
					if (e_global->var_bytes < 0)
						 e_global->var_bytes = 0;
					 t = time(NULL);
				}
			}
		}
		
reconnect:

#ifdef VV_DEBUG
		enter_vvd_section("reconnect");
#endif
		
		close(e_global->var_socket);
		sleep(set_get_int("connect_sleep"));

		log(LOG_DEBUG, "Trying next server..");

		{
			DListElmt *el;
			el = dlist_find(&e_global->servers, e_global->server);
			
			if (el == NULL)
			{
				sleep(set_get_int("exhaused_sleep"));
				e_global->server = e_global->servers.head->data;
			}
			else
			{
				e_global->server = (el->next != NULL)?
					(el->next->data):
					(e_global->servers.head->data);
			}
			
		}
	}

	return 0;
}
