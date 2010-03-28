/* lol module for eiwic because life is grey
   
   lordi@styleliga.org
 */
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

EP_SETNAME("lol");

int ep_parse(MSG *ircmsg)
{
	char mybuf[50];

	if (strcmp(ircmsg->cmd, "privmsg") == 0)
	{
		if (strstr(ircmsg->args, "around a bit with a large") &&
			strstr(ircmsg->args, e_global->botnick))
		{
			eiwic->plug_sendf("PRIVMSG %s :\001ACTION slaps %s around a bit with a large cucumber\001\r\n",
				ircmsg->to, ircmsg->nick);
		}
		else
		if (strstr(ircmsg->args, "lol"))
		{
			if (rand() % 8 == 1)
				eiwic->plug_sendf("PRIVMSG %s :haha\r\n", ircmsg->to);
		}
	}
}
