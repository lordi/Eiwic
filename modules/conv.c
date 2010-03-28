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

EP_SETNAME("conv");

typedef struct cyborg_request {
	OUTPUT *out;
	double value;
} REQ;

int usage(OUTPUT *out)
{
	eiwic->output_printf(out, "usage: !conv <from> <to> <value>\n");
	eiwic->output_printf(out, "  <value> is the value to convert (eg. 2.50)\n");
	eiwic->output_printf(out, "  <from> and <to> both are 3-letter-currency IDs:\n");
	eiwic->output_printf(out, "    EUR, USD, DEM, GBP, NOK, ...\n");
}

int ep_help(OUTPUT *out)
{
	eiwic->output_printf(out, "The convert module is a simple gateway to a www\n");
	eiwic->output_printf(out, "page (www.oanda.com) to convert a value from a currency\n");
	eiwic->output_printf(out, "to another.\n");

	usage(out);
	
	return 1;
}

static void remove_tags(char *s)
{
	char *p, *t;
	
	p = s;
	
	while (*p)
	{
		switch(*p) {
			case '>':
				{
					memmove(s, p + 1, strlen(p));
					p = s;
					break;
				}
			case '<':
				if ((t = strchr(p, '>')) != NULL)
				{
					if (strncmp(p, "<B>", 3) == 0 ||
						strncmp(p, "</B>", 4) == 0)
					{
						memmove(p + 1, t + 1, strlen(t));
						*p = '\002';
					}
					else
					{				
						memmove(p, t + 1, strlen(t));
					}
					break;
				}
			case '&':
				if (strncmp(p+1, "nbsp;", 5) == 0)
				{
					memmove(p+1, p+6, strlen(p+5));
					*p = ' ';
					break;
				}				
			default:
				++p;
		}		
	}
}

int convert_kurs(char *retrieved, void *data)
{
	REQ *req = data;
	char *p,*q;

	p = strstr(retrieved, "<td VALIGN=\"TOP\"><P>");
	q = strstr(retrieved, "<P> <small>");

	if (!p || !q)
	{
		eiwic->output_print(req->out, "conv: error.\n");
		return -1;
	}

	*q = 0;

	remove_tags(p);

	eiwic->output_print(req->out, p);

	return 1;
}

int convert(char *parameter, OUTPUT *out, MSG *msg)
{
	REQ *r;
	char http[170];
	char *from, *to, *val;
	
	r = malloc(sizeof(REQ));
	
	if (parameter == NULL)
	{
		return usage(out);
	}

	if ((from = strtok(parameter, " ")) == NULL)
	{
		return usage(out);
	}

	if ((to = strtok(NULL, " ")) == NULL)
	{
		return usage(out);
	}

	if ((val = strtok(NULL, " ")) == NULL)
	{
		return usage(out);
	}

	if ( (r->value = strtod(val, NULL)) <= (double)0
		|| (r->value > (double)10000000) )
	{
		return usage(out);
	}
	
	if (strlen(from) != 3 || strlen(to) != 3)
	{
		return usage(out);
	}

	from[0] = toupper(from[0]);
	from[1] = toupper(from[1]);
	from[2] = toupper(from[2]);

	to[0] = toupper(to[0]);
	to[1] = toupper(to[1]);
	to[2] = toupper(to[2]);

	r->out = out;

	sprintf(http,
			"http://www.oanda.com/convert/classic?"
			"user=printable&exch=%s&value=%.2f&expr=%s", from, r->value, to);

			
	mlink->conn_http_get(http, convert_kurs, r);
	
	return 0;
}

/* plugin main function, register !conv trigger */
int ep_main(OUTPUT *out)
{
	eiwic->plug_trigger_reg("!conv", TRIG_PUBLIC, convert);
	
	return 1;
}
