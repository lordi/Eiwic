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

#include "eiwic.h"
#include "plugins.h"

EP_SETNAME("info");

int ep_help(OUTPUT *out)
{
	eiwic->output_print(out, "Info is a module made to bring you the following\n");
	eiwic->output_print(out, "triggers: !up (shows Eiwics current uptime), !info (more\n");
	eiwic->output_print(out, "information on the running bot) and !help.\n");
}

int on_info(char *pa, OUTPUT *out, MSG *ircmsg)
{
	eiwic->output_printf(out, 
			NAME " " VERSION " got %i modules / %i triggers\n",
			e_global->plugins.size,
			e_global->triggers.size);
	
	return 1;
}

int on_uptime(char *pa, OUTPUT *out, MSG *ircmsg)
{
	int diff = time(NULL) - e_global->time_launched,
		d = (int)((diff) / 86400),
		h = (int)((diff - d * 86400) / 3600),
		m = (int)((diff - d * 86400 - h * 3600) / 60);
	
	eiwic->output_printf(out,
			NAME " " VERSION " up for %d days, %d hrs, %d mins\n",
			d, h, m);

	return 1;
}

int on_help(char *param, OUTPUT *out, MSG *ircmsg)
{
	PLUGIN *p;
	TRIGGER *t;
	int i = 0;
	DListElmt *el, *elt;

	if (param == NULL)
	{
		eiwic->output_print(out, "Eiwic is an IRC-bot, which can easily be extended by\n");
		eiwic->output_print(out, "modules. And it likes you.\n");
		eiwic->output_print(out, "For more information: " EIWIC_HOMEPAGE "\n");
		
		FOR_EACH(&e_global->plugins, el)
		{
			p = el->data;

			if (p->ep_help == NULL) continue;

			if (i++ == 0)
			{
				eiwic->output_print(out, "You can get more help by using '!help topic', where topic\n");
				eiwic->output_print(out, "is one of the following: ");
			}
			else
			{
				eiwic->output_print(out, ", ");
			}

			eiwic->output_printf(out, "%s", p->name);
		}

		if (i == 0)
		{
			eiwic->output_print(out, "Sorry, no more help available.\n");
		}
		else
		{
			eiwic->output_print(out, ".\n");
			eiwic->output_print(out, "Additionally, you can type '!help !triggername'.\n");
		}
	}
	else
	{
		FOR_EACH(&e_global->plugins, el)
		{
			p = el->data;

			if (p->ep_help == NULL) continue;
			if (strcmp(p->name, param) != 0) continue;

			i++;

			p->ep_help(out);
		}

		if (i == 0)
		{
			FOR_EACH(&e_global->triggers, el)
			{
				t = el->data;

				if (t->plugin == NULL) continue;
				if (t->plugin->ep_help == NULL) continue;
				if (strcmp(t->trigger, param) != 0) continue;
	
				i++;

				eiwic->output_printf(out, "Trigger %s is registered by the module '%s':\n",
					t->trigger, t->plugin->name);
				t->plugin->ep_help(out);
			}
		}

		if (i == 0)
		{
			eiwic->output_printf(out, "Sorry, no help available for '%s'.\n", param);
		}
	}
		
	return 1;
}

int ep_main(OUTPUT *out)
{	
	mlink->plug_trigger_reg("!info", TRIG_PUBLIC, on_info);
	mlink->plug_trigger_reg("!help", TRIG_PUBLIC, on_help);
	mlink->plug_trigger_reg("!up", TRIG_PUBLIC, on_uptime);
	
	return 1;
}

