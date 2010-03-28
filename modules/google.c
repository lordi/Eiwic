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

#define GF_SUCCESS "Try: %s\n"
#define GF_FAILED "google: Nothing has been found... :|\n"
#define GF_USAGE "usage: !google <something> or !find <something>\n"

EP_SETNAME("google");

typedef struct google_request {
	OUTPUT *out;
	char request[128];
	CONN *conn;
} REQ;

int ep_help(OUTPUT *out)
{
	eiwic->output_printf(out, "The Google module should be self-explaining :).\n");
	eiwic->output_printf(out, GF_USAGE);
	return 1;
}

int google_found(char *retrieved, void *data)
{
	REQ *req = data;
	char *p;

	if ((p = strstr(((HTTP*)(req->conn->data))->buffer, "Location: ")) != NULL)
	{
		eiwic->output_printf(req->out, GF_SUCCESS, strtok(p + 10, "\r\n"));
	}
	else
	{
		eiwic->output_printf(req->out, GF_FAILED);
	}
	free(req);

	return 1;
}

int find(char *parameter, OUTPUT *out, MSG *msg)
{
	REQ *r;
	char http[170], *p;
		
	if (parameter == NULL)
	{
		eiwic->output_print(out, GF_USAGE);
		return 0;
	}

	r = malloc(sizeof(REQ));
	r->out = out;
	strncpy(r->request, parameter, 127);
	p = r->request;

	while (*p++) if (*p == ' ') *p = '+';

	sprintf(http,"http://www.google.com/search?btnI=&q=%s", r->request);

	r->conn = eiwic->conn_http_get(http, google_found, r);
	return 1;
}

int ep_main(OUTPUT *out)
{
	mlink->plug_trigger_reg("!find", TRIG_PUBLIC, find);
	mlink->plug_trigger_reg("!google", TRIG_PUBLIC, find);

	return 1;
}

