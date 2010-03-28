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

EP_SETNAME("admin");

int ep_help(OUTPUT *out)
{
	eiwic->output_print(out, "The Admin plugin offers triggers for administrational management.\n");
	eiwic->output_print(out, "Of course, they can only be used as an bot admin:\n");
	eiwic->output_print(out, "> !join <chan>, !part (<chan>), !lschan, !raw <rawmsg>,\n");
	eiwic->output_print(out, "> !msg <channel/nick> <something>\n");
}

int on_lschan(char *parameter, OUTPUT *out, MSG *ircmsg)
{
	CHANNEL *chan;

	FOR_EACH_DATA(&e_global->channels, chan)
	{
		if (chan->joined)
		{
			eiwic->output_printf(out, "%s\n", chan->name);
		}
	}
	END_DATA;

	return 1;
}

int on_join(char *parameter, OUTPUT *out, MSG *ircmsg)
{
	if (parameter)
		eiwic->output_printf(e_global->server_output, "JOIN %s\n", parameter);
	else
		eiwic->output_printf(out, "usage: !join <channel>\n");

	return 1;
}

int on_part(char *parameter, OUTPUT *out, MSG *ircmsg)
{
	CHANNEL *c;

	if (parameter)
		c = eiwic->channel_find(parameter);
	else
		c = ircmsg->channel;

	if (c == NULL)
	{
		eiwic->output_printf(out, "part: Please specify a channel.\n");
		return 0;
	}

	eiwic->output_printf(e_global->server_output, "PART %s\n", c->name);
}

int on_raw(char *parameter, OUTPUT *out, MSG *ircmsg)
{
	if (parameter)
	{
		eiwic->output_print(e_global->server_output, parameter);
		eiwic->output_print(e_global->server_output, "\n");
	}
	else
		eiwic->output_printf(out, "usage: !raw <something>\n");
}

int on_msg(char *parameter, OUTPUT *out, MSG *ircmsg)
{
	if (parameter)
	{
		char *p = strtok(parameter, " "), *q;

		if (p)
		{
			q = strtok(NULL, "");

			if (q)
			{
				eiwic->output_print(e_global->server_output, "PRIVMSG ");
				eiwic->output_print(e_global->server_output, p);
				eiwic->output_print(e_global->server_output, " :");
				eiwic->output_print(e_global->server_output, q);
				eiwic->output_print(e_global->server_output, "\n");
				return 1;
			}
		}
	}

	eiwic->output_printf(out, "usage: !msg <channel/nick> <something>\n");
}

int ep_main(OUTPUT *out)
{	
	mlink->plug_trigger_reg("!msg", TRIG_ADMIN, on_msg);
	mlink->plug_trigger_reg("!raw", TRIG_ADMIN, on_raw);
	mlink->plug_trigger_reg("!join", TRIG_ADMIN, on_join);
	mlink->plug_trigger_reg("!part", TRIG_ADMIN, on_part);
	mlink->plug_trigger_reg("!lschan", TRIG_ADMIN, on_lschan);
	
	return 1;
}

