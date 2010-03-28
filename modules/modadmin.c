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

EP_SETNAME("modadmin");

int ep_help(OUTPUT *out)
{
	eiwic->output_print(out, "The modadmin plugin offers triggers for runtime module\n");
	eiwic->output_print(out, "loading. The following triggers can be used by a bot\n");
	eiwic->output_print(out, "admin: !lsmod (list modules), !insmod <module> (load module\n");
	eiwic->output_print(out, "at runtime), !rmmod <module> (unload module).\n");
}

int on_insmod(char *parameter, OUTPUT *out, MSG *ircmsg)
{
	PLUGIN *p;

	if (parameter == NULL)
	{
		eiwic->output_print(out, "usage: !insmod <module>\n");
		return 0;
	}

	p = eiwic->plug_load(parameter, out);

	if (p == NULL)
	{
		eiwic->output_printf(out, "insmod: Error while loading '%s'! [see log file]\n", parameter);
		eiwic->output_printf(out, "insmod: Possible reasons: Module does not exist or is loaded already.\n");
	}
	else
	{
		eiwic->output_printf(out, "insmod: added module \002%s\002 (path: %s)\n", p->name, p->filename);
	}

	return 1;
}

int on_rmmod(char *param, OUTPUT *out, MSG *ircmsg)
{
	PLUGIN *p;

	if (param == NULL)
	{
		eiwic->output_printf(out, "usage: !rmmod <modulename>\n");
		return 0;
	}

	FOR_EACH_DATA(&e_global->plugins, p)
	{
		if (strcmp(p->name, param) == 0) break;
		else p = NULL;
	}
	END_DATA;
	
	if (p == NULL)
	{
		eiwic->output_printf(out, "rmmod: can't find module '%s' (hint: !lsmod).\n", param);
	}
	else
	{
		eiwic->output_printf(out, "rmmod: unloading %s..\n", p->name);
		eiwic->plug_unload(p);
	}

	return 1;
}

int on_lsmod(char *parameter, OUTPUT *out, MSG *ircmsg)
{
	PLUGIN *p;
	int i = 0;
	DListElmt *el;

	eiwic->output_printf(out, "loaded modules: ");

	FOR_EACH(&e_global->plugins, el)
	{
		p = el->data;
		if (i++ != 0) eiwic->output_print(out, ", ");
		eiwic->output_printf(out, "%s", p->name);
	}

	eiwic->output_print(out, "\n");
}

int on_reload(char *param, OUTPUT *out, MSG *ircmsg)
{
	if (param == NULL)
	{
		eiwic->output_printf(out, "usage: !reload <modulename>\n");
		return 0;
	}
	
	if (on_rmmod(param, out, ircmsg))
		return on_insmod(param, out, ircmsg);
	else
		return 0;
}

int ep_main(OUTPUT *out)
{	
	mlink->plug_trigger_reg("!lsmod", TRIG_ADMIN, on_lsmod);
	mlink->plug_trigger_reg("!insmod", TRIG_ADMIN, on_insmod);
	mlink->plug_trigger_reg("!rmmod", TRIG_ADMIN, on_rmmod);
	mlink->plug_trigger_reg("!reload", TRIG_ADMIN, on_reload);
	
	return 1;
}

