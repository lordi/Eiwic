/******************************************************************************
 * eiwic - Extensible Ircbot Written In C
 * Copyright (C) Hannes Gräuler <lordi@styleliga.org>
 *
 * settings.c: Implementation of functions to load and validate the config
 *             file.
 ****************************************************************************/

#include "eiwic.h"
#include "settings.h"
#include "plugins.h"
#include "servers.h"

static int conf_set(char *parameter, char *value);
static int conf_add(char *parameter, char *value);
static int conf_echo(char *parameter, char *value);
static int conf_load(char *parameter, char *value);
static int conf_load_module(char *filename);

int	conf_loadfile()
{
	FILE* conf;
	char buf[512], *action, param[128];
	int line = 0, success;
	
	if (!(conf = fopen(set_get_string("config_file"), "r")))
	{
		log(LOG_ERROR, "Unable to open config file \"%s\" for reading",
				set_get_string("config_file"));
		return -1;
	}
	
	while (fgets(buf, sizeof(buf), conf) > 0 && ++line)
	{
		if (!*buf || *buf=='#')
			continue;
		
		action = strtok(buf, " \t\r\n");
		if (action != NULL)
		{
			if (strcasecmp(action, "set") == 0)
			{
				sprintf(param, "%s", strtok(NULL, " =\r\n"));
				success = conf_set(param, strtok(NULL, "\r\n"));
			}
			else if (strcasecmp(action, "add") == 0)
			{
				sprintf(param, "%s", strtok(NULL, " \r\n"));
				success = conf_add(param, strtok(NULL, "\r\n"));
				
			}
			else if (strcasecmp(action, "echo") == 0)
			{
				success = conf_echo(strtok(NULL, "\r\n"), NULL);
			}
			else if (strcasecmp(action, "load") == 0)
			{
				sprintf(param, "%s", strtok(NULL, " =\r\n"));
				success = conf_load(param, strtok(NULL, "\r\n"));
			}
			else
			{
				log(LOG_ERROR, "configuration file: Unknown command \"%s\"",
						action);
				success = -1;
			}
			
			if (success == -1)
			{
				log(LOG_WARNING, "Loading of configuration file failed "
						"(error in %s, line %i)", set_get_string("config_file"), 
						line);
				fclose(conf);
				return -1;
			}
		}
	}
	
	fclose(conf);
	return 0;
}

int conf_set(char *parameter, char *value)
{
	SETTING *s;
	
	if ((s = set_find(parameter)) == NULL)
	{	/* can only set existing settings */
		log(LOG_ERROR, "No such configuration symbol: %s", parameter);
		return -1;
	}

	switch (s->type)
	{
	case SET_BOOL:
		set_set(s, (strcmp(value, "yes") == 0 && atoi(value) == 1)?1:0);
		break;
	case SET_FLOAT:
		set_set(s, atof(value));
		break;
	case SET_INT:
		set_set(s, atol(value));
		break;
	case SET_STRING:
	case SET_LONGSTRING:
		set_set(s, value);
		break;
	}
	
	return 0;
}

int conf_echo(char *parameter, char *value)
{
	log(LOG_STATUS, "config: %s\n", parameter);
	return 0;
}

int conf_add(char *parameter, char *value)
{
	if (strcasecmp(parameter, "channel") == 0)
	{
		channel_add(value);		
	}
	else
	if (strcasecmp(parameter, "server") == 0)
	{
		char host[128], port[6], *p, ipv6=0;


		if ((p = strtok(value, " \r\n")) == NULL)
			return 0;

		strncpy(host, p, sizeof(host));

		
		if ((p = strtok(NULL, " \r\n")) == NULL)
			return 0;

		strncpy(port, p, sizeof(port));

	
		p = strtok(NULL, " \r\n");

		if (p)
			if (strcasecmp(p, "ipv6") == 0)
				ipv6 = 1;

#ifndef IPV6
		if (ipv6 == 1)
		{
			log(LOG_WARNING, "Unable to add IPv6 server '%s'! "
					"You don't have IPv6 support enabled.", host);
			return 0;
		}
#endif

		server_add(host, (u_short)atoi(port), ipv6);
	}
	else
	{
		log(LOG_ERROR, "add: Unknown parameter \"%s\"", parameter);
		return -1;
	}

	return 0;
}

int conf_load(char *parameter, char *value)
{
	if (strcasecmp(parameter, "module") == 0)
	{
		/*char fn[1024];
		sprintf(fn, "%s%s", set_get_string("module_path"), value);
		*/
		
		log(LOG_INIT_DEBUG, "Loading module %s...", value);
				
		return conf_load_module(value);
	}
	else
	{
		log(LOG_ERROR, "load: Unknown parameter \"%s\"", parameter);
		return -1;
	}
}

int conf_load_module(char *filename)
{
	struct eiwic_plugin *ep;
		
	ep = plug_load(filename, e_global->log_output);
	
	if (ep != NULL)
	{
		return 0;
	}
	else
	{
		log(LOG_ERROR, "Loading of module %s failed: %s", filename, dlerror());
		return -1;
	}
}
