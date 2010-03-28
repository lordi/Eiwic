/* Eiwics NickServ authentication module */
/* autor: Florian Scholz (florian90@gmail.com) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include "eiwic.h"
#include "plugins.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

EP_SETNAME("nickserv");

int ep_help(OUTPUT *out)
{
        eiwic->output_print(out, "nickserv authenticates the eiwic bot with the nickserv irc service.\n");
}

int ep_parse(MSG *ircmsg)
{
	if(!strcmp(ircmsg->cmd,"notice"))
        {
                if((!strcmp(ircmsg->user->nick,"NickServ")) && (!strcmp(ircmsg->to,eiwic->set_get_string("NICKSERV_USER"))))
                {
  			char *type = eiwic->set_get_string("NICKSERV_TYPE");
			char *pattern;
			if(!strcmp(type,"anope_1_8_3_german"))
			{
				pattern = "einen anderen Nickname.";
			}
			else if(!strcmp(type,"freenode"))
			{
				pattern = "This nickname is registered. Please choose a different nickname, or identify via \002/msg NickServ identify <password>\002.";
			}
			else
			{
				eiwic->log(LOG_ERROR,"Couldn't recognize the NickServ type given by NICKSERV_TYPE.");
				return 1;
			}
                        if(!strcmp(ircmsg->args,pattern))
                        {
                                mlink->plug_sendf("PRIVMSG NickServ :identify %s\n",eiwic->set_get_string("NICKSERV_PASS"));
                        }
                }
        }

}
int ep_unload(OUTPUT *out)
{
        return 1;
}

int ep_main(OUTPUT *out)
{        
        return 1;
}

