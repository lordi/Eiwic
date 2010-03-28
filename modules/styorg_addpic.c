#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <dlfcn.h>

#include "eiwic.h"
#include "plugins.h"

#define ADDPIC_USAGE "usage: [!addfunny|!addsexy|!addmisc] <url>\n"
#define ADDPIC_URL "http://www.styleliga.org/addpic.php?cat_id=%d&nickname=%s&url=%s"

EP_SETNAME("styorg_addpic");

int ep_help(OUTPUT *out)
{
	eiwic->output_printf(out, "Tool module to add a picture to the www.styleliga.org\n");
	eiwic->output_printf(out, "picture database.\n");
	eiwic->output_printf(out, ADDPIC_USAGE);
	return 1;
}

int pic_added(char *retrieved, void *data)
{
	OUTPUT *out = data;
	eiwic->output_print(out, retrieved);
	return 1;
}

int addpic(char *parameter, int section, OUTPUT *out, MSG *msg)
{
	char http[370];
	
	if (parameter == NULL || strlen(parameter) > 300 || strncmp(parameter, "http://", 7) != 0)
	{
		eiwic->output_print(out, ADDPIC_USAGE);
		return 0;
	}
	
	sprintf(http,ADDPIC_URL, section, msg->user->nick, parameter);
	eiwic->conn_http_get(http, pic_added, out);
	
	return 1;
}

int on_addpic_funny(char *parameter, OUTPUT *out, MSG *msg)
{
	return addpic(parameter, 3, out, msg);	
}

int on_addpic_misc(char *parameter, OUTPUT *out, MSG *msg)
{
	return addpic(parameter, 4, out, msg);	
}

int on_addpic_sexy(char *parameter, OUTPUT *out, MSG *msg)
{
	return addpic(parameter, 5, out, msg);	
}

int ep_main(OUTPUT *out)
{
	mlink->plug_trigger_reg("!addsexy", TRIG_PUBLIC, on_addpic_sexy);
	mlink->plug_trigger_reg("!addfunny", TRIG_PUBLIC, on_addpic_funny);
	mlink->plug_trigger_reg("!addmisc", TRIG_PUBLIC, on_addpic_misc);

	return 1;
}

