#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <dlfcn.h>

#include <sys/stat.h>
#include <dirent.h>

#include "eiwic.h"
#include "plugins.h"

#define STATS_DIR "stats"

EP_SETNAME("stats");

int ep_help(OUTPUT *out)
{
	eiwic->output_printf(out, "The stats module manages statistics about how\n");
	eiwic->output_printf(out, "many times a trigger has been called. The information\n");
	eiwic->output_printf(out, "is saved in " STATS_DIR "/.\n");
	eiwic->output_printf(out, "usage: !stats <!trigger>\n");
}

int isdir (char *name)	
{
	struct stat buf;
	
	if (stat(name, & buf))
		return 0;
		
	return (buf.st_mode & S_IFDIR) ? 1 : 0;
}

int showinfo(char *trigger, char *filename, OUTPUT *out)
{
	FILE *f;
	char buf[1024], *sample;

	if (((f = fopen(filename, "r")) != NULL) &&
	   (fgets(buf, sizeof(buf) - 1, f) > 0))
	{
		if ((sample = strchr(buf, ' ')) != NULL)
		{
			*sample++='\0';
			eiwic->output_printf(out, "Trigger %s used %s times, "
					"last: %s\n", trigger, buf, sample);
		}
	}
	else
		eiwic->output_printf(out, "No information on %s available yet.\n", trigger);
	if (f != NULL) fclose(f);					
		
}

int stats(char *parameter, OUTPUT *out, MSG *msg)
{
	DIR *dir;	
	char *p=parameter;
	char buf[1024];
	
	if (parameter && (strchr(parameter, '/') == NULL))
	{
		for(;*p;++p) *p = tolower(*p);
		strcpy(buf, STATS_DIR"/");
		strncpy(buf+strlen(buf), parameter, 1000);
		buf[1023]=0;
		
		showinfo(parameter, buf, out);
	}		
	else
	{
#if 0
		if ((dir = opendir(STATS_DIR)) != NULL)
		{
			struct dirent *de;

			while (de = readdir(dir))
			{
				if (strcmp(de->d_name, ".") == 0 ||
					strcmp(de->d_name, "..") == 0)
					continue;

				/*
				mlink->plug_sendf("PRIVMSG %s :stats: %s\r\n",
					msg->to, de->d_name);
				*/

				strcpy(buf, STATS_DIR"/");
				strncpy(buf+strlen(buf), de->d_name, 1000);
				buf[1023]=0;
		
				showinfo(de->d_name, buf, msg);
			}		
			closedir(dir);
		}
		else
		{
			mlink->plug_sendf("PRIVMSG %s :stats: No information available yet.\r\n",
				msg->to);		
		}
#endif
	}
	return 1;
}

int ep_trigger(TRIGGER *trigger, OUTPUT *out, MSG *msg)
{	
	char fname[128], buf[15];
	long counted;
	FILE *f;
	
	if (msg == NULL) return 0;

	if (isdir(STATS_DIR) == 0)
	{
		if (mkdir(STATS_DIR, 0700) == -1)
		{
			mlink->log(LOG_ERROR, "Could not access or create directory 'stats'");
			return -1;
		}
	}
	
	sprintf(fname, STATS_DIR"/%s", trigger->trigger);
	
	if ((f = fopen(fname, "r")) != NULL)
	{
		if (fgets(buf, sizeof(buf) - 1, f) > 0)			
			counted = atol(buf);
		else			
			counted = 0;		
		fclose(f);
	}
	else
		counted = 0;

	if ((f = fopen(fname, "w")) == NULL)
		return -1;

	fprintf(f, "%i <%s> %s\n", ++counted, msg->nick, msg->args);
	fclose(f);
	
	if (counted % 100 == 0)
	{
		eiwic->output_printf(out, "*** \002Congratulations, %s, your the %d. person who used %s!\002"
		"You will get a free copy of the Eiwic-Collectors-DVD! Rock'n Roll ***\n",
		msg->nick, counted, trigger->trigger);
	}
}

int ep_main(OUTPUT *out)
{
	eiwic->plug_trigger_reg("!stats", TRIG_PUBLIC, stats);

	return 1;
}

