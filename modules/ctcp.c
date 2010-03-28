/*
 *	ctcp for the eiwic bot
 * 
 *  by lordi@styleliga.org | http://lordi.styleliga.org/
 *
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

#define EIWIC_CTCP_VERSION NAME "/" VERSION " " EIWIC_HOMEPAGE

EP_SETNAME("ctcp");

int ep_parse(MSG *ircmsg)
{
	char *h1,*h2;
	
	if (strcmp(ircmsg->cmd, "privmsg") == 0)
	{
		if (*ircmsg->args == '\01')
		{
			h1 = ircmsg->args + 1;
			
			h2 = strchr(h1, '\01');

			if (h2 == NULL) return 0;
				
			*h2 = 0;
			

			h2 = strtok(h1, " ");
			

			if (h2 == NULL) return 0; /* THX 2 millhouse :> */

			if (strcasecmp(h2, "version") == 0)
			{
				mlink->plug_sendf(
						"NOTICE %s :\01VERSION %s\01\r\n", ircmsg->nick, EIWIC_CTCP_VERSION);
			}
			if (strcasecmp(h2, "ping") == 0)
			{

				if (strlen(h1+strlen(h2)+1) < 40)
				mlink->plug_sendf(
						"NOTICE %s :\01PING %s\01\r\n", ircmsg->nick, h1 + strlen(h2) + 1);
			}
			/*
				TODO
				----------------

				TIME
				FINGER
				CLIENTINFO

			*/
		}
	}

	return 1;
}
