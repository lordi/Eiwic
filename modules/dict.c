/*
 * dict module for eiwic
 *
 *    this uses the dict.leo.org www interface to translate
 *    words between german and english
 *
 * update: oct 15th 2004 | lordi@styleliga.org
 *    - changed module code to use (OUTPUT *)
 *
 * update: oct 9th 2004 | lordi@styleliga.org
 *    - now using pda.leo.org
 *    - added !dico for french - german translation
 *    - had to update html parsing
 *    - support for <b>-tag
 *
 * update: mar 28th 2010 | florian90@gmail.com
 *    - renamed !dico to !dict_fr and !dict to !dict_en
 *    - added !dict_es for spanish - german translation
 *    - added !dict_ch for chinese - german translation
 *
 * by lordi@styleliga.org | http://lordi.styleliga.org/ 2002
 * extensions by florian90@gmail.com / 2010
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

EP_SETNAME("dict");

#define DICTEN_CODE "ende"
#define DICTFR_CODE "frde"
#define DICTES_CODE "esde"
#define DICTCH_CODE "chde"

#define DICT_MAX_TRANSLATIONS 5

#define HTMLANC_START "<input type=\"hidden\" name=\"lp\" "
#define HTMLANC_FIRST "<td class=\"td1\" valign=\"middle\" width=\"4"
#define HTMLANC_SECOND HTMLANC_FIRST

#define USAGE "usage: !dict_en <english or german word>\n" \
              "       !dict_fr <french or german word>\n" \
	      "       !dict_es <spanish or german word>\n" \
	      "       !dict_ch <chinese or german word>\n"

typedef struct dictionary_request {
	OUTPUT *out;
	char request[42];
} REQ;

int ep_help(OUTPUT *out)
{
	eiwic->output_print(out, "The dict module is using the dict.leo.org www-interface\n");
	eiwic->output_print(out, "to translate single words.\n");
	eiwic->output_print(out, USAGE);

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

int dict_looked_up(char *retrieved, void *data)
{
	REQ *req = data;
	char *p,*q,*r, *s;
	char buf[1024];
	int i,show=0;
	
	p = strstr(retrieved, HTMLANC_START);

	for (i = 0, buf[0] = '\0'; p; i++)
	{
		q = strstr(p, HTMLANC_FIRST);
		
		if (q == NULL)
			break;		
	
		q += strlen(HTMLANC_FIRST);
		r = strstr(q, "</td>");
		*r = 0;
		
		s = strstr(r + 1, HTMLANC_SECOND);
		s += strlen(HTMLANC_SECOND);
		
		r = strstr(s, "</td>");
		*r = 0;

		p = r + 1;
		
		if (i < DICT_MAX_TRANSLATIONS)
		{
			show = i + 1;

			remove_tags(q);
			strcat(buf, q);

			strcat(buf, " <-> ");

			remove_tags(s);
			strcat(buf, s);
		
			strcat(buf, "\n");
		}
	}
	
	if (i == 0)
	{
		eiwic->output_printf(req->out, "not found: %s\n", req->request);
		return 0;
	}
	else
	{
		eiwic->output_printf(req->out,
			"-------------- %s (%d/%d) --------------\n",
			req->request, show, i);

		eiwic->output_print(req->out, buf);	
	}

	return 1;
}

int dict(char *search, char *lp, OUTPUT *out)
{
	REQ *r;
	char http[250];
	int i;
		
	if (search == NULL)
	{
		eiwic->output_print(out, USAGE);
		return -1;
	}
	
	r = malloc(sizeof(REQ));
	strncpy(r->request, search, 41);
	r->out = out;

	/* replace all spaces by '+' */
	for (i = 0; search[i]; i++) if (search[i] == ' ') search[i] = '+';

	/* no longer strings than 50 chars */
	if (i > 50) search[50] = 0; 

	/* put together URL */
	sprintf(http, "http://pda.leo.org/?lp=%s&lang=de&searchLoc=0&cmpType=relaxed"
	              "&relink=off&sectHdr=off&spellToler=std&search=%s", lp, search);
	              
	mlink->conn_http_get(http, dict_looked_up, r);
	
	return 1;
}

int dict_en(char *parameter, OUTPUT *out, MSG *msg)
{
	return dict(parameter, DICTEN_CODE, out);
}

int dict_fr(char *parameter, OUTPUT *out, MSG *msg)
{
	return dict(parameter, DICTFR_CODE, out);
}
int dict_es(char* parameter, OUTPUT *out, MSG *msg)
{
	return dict(parameter,DICTES_CODE,out);
}
int dict_ch(char* parameter, OUTPUT *out, MSG *msg)
{
	return dict(parameter,DICTCH_CODE,out);
}
int ep_main(OUTPUT *out)
{
	mlink->plug_trigger_reg("!dict_en", TRIG_PUBLIC, dict_en);
	mlink->plug_trigger_reg("!dict_fr", TRIG_PUBLIC, dict_fr);
	mlink->plug_trigger_reg("!dict_es", TRIG_PUBLIC, dict_es);
	mlink->plug_trigger_reg("!dict_ch",TRIG_PUBLIC, dict_ch);
	return 1;
}
