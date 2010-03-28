#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <dlfcn.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>

#include "eiwic.h"
#include "plugins.h"
#include "numerics.h"

EP_SETNAME("getnick");

OUTPUT *msg_out;
TIMER *nick_trig;
char nick[128];
int minutes = 0;

void nick_get_now(void *data)
{
	eiwic->output_printf(e_global->server_output, "NICK %s\n", nick);
//	eiwic->output_printf(msg_out, "(debug) NICK %s\n", nick);

	minutes++;
	minutes *= 2;
	if (minutes > 30) minutes = 30;
	nick_trig = eiwic->plug_timer_reg(time(NULL) + minutes * 60, nick_get_now, NULL);
}

int ep_parse(MSG *ircmsg)
{
	if (*nick == 0) return 1;

	if (ircmsg->cmd_num == 0)
	{
		if (strcmp(ircmsg->cmd, "quit") == 0) {
			if (strcmp(ircmsg->user->nick, nick) == 0)
			{
				eiwic->log(LOG_STATUS, "< %s[*@%s] QUITTED (%s). TRYING TO GET NICK NOW!",
					ircmsg->user->nick,
					ircmsg->host,
					ircmsg->args);
			
				eiwic->output_printf(msg_out, "getnick: %s[*@%s] quitted (%s).\n",
					ircmsg->user->nick,
					ircmsg->host,
					ircmsg->args);
			
				minutes = 0;
				if (nick_trig) eiwic->plug_timer_unreg(nick_trig);
				nick_get_now(NULL);
			}
		}
		else
		if (strcmp(ircmsg->cmd, "nick") == 0) {
			if (strcmp(ircmsg->user->nick, e_global->botnick) == 0
				&& strcmp(ircmsg->args, nick) == 0)
			{
				eiwic->log(LOG_STATUS, "Success, finally got nick %s! It was a pleasure for me. ;)",
					nick);
				if (nick_trig) eiwic->plug_timer_unreg(nick_trig);
				nick_trig = NULL;
			}
			else
			if (strcmp(ircmsg->user->nick, nick) == 0)
			{
				eiwic->log(LOG_STATUS, "< %s[*@%s] CHANGED HIS NICK (TO %s). TRYING TO GET NICK NOW!",
					ircmsg->user->nick,
					ircmsg->host,
					ircmsg->args);
				
				eiwic->output_printf(msg_out, "getnick: %s[*@%s] changed nick.\n",
					ircmsg->user->nick,
					ircmsg->host);
				
				minutes = 0;
				if (nick_trig) eiwic->plug_timer_unreg(nick_trig);
				nick_get_now(NULL);
			}
		}
	}
	else
	switch (ircmsg->cmd_num)
	{
	case ERR_NICKNAMEINUSE:
//			eiwic->output_printf(msg_out, "(debug) %s\n", ircmsg->args);
		break;
	case 437:
			eiwic->output_printf(msg_out, "%s\n", ircmsg->args);
		break;
	}
}

int on_getnick(char *parameter, OUTPUT *out, MSG *ircmsg)
{
	msg_out = out;
	strncpy(nick, parameter, 127);
	nick[127] = 0;
	if (nick_trig) eiwic->plug_timer_unreg(nick_trig);
	eiwic->output_printf(out, "getnick: Target nick set to '%s'\n", nick);
	minutes = 0;
	nick_get_now(NULL);
}

int ep_unload(OUTPUT *out)
{
	if (nick_trig) eiwic->plug_timer_unreg(nick_trig);
}

int ep_main(OUTPUT *out)
{	
	*nick = 0;
	mlink->plug_trigger_reg("!getnick", TRIG_ADMIN, on_getnick);
	msg_out = NULL;
	
	return 1;
}

