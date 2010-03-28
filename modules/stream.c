/* Eiwic IRC2WWW stream, by lordi@styleliga.org */
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

#include "eiwic.h"
#include "plugins.h"

#define TITLE "%s - Eiwic Live stream v1.2"
#define MAX_CLIENTS 40
#define MAX_LINES 20
#define STREAM_PORT 1600

EP_SETNAME("stream");

struct _stream_client {
	char used;
	CONN *conn;
	CHANNEL *chan;
} stream_clients[MAX_CLIENTS];

struct _stream_channel {
	CHANNEL *chan;
	char *lines[MAX_LINES];
} **stream_channels;

int ep_help(OUTPUT *out)
{
	eiwic->output_printf(out, "The stream module allows you to view the channel\n");
	eiwic->output_printf(out, "via a HTTP connection. It is configured to run on\n");
	eiwic->output_printf(out, "port %d.\n", STREAM_PORT);		

	eiwic->output_printf(out, "So, try 'http://<MYIP>:%d/chan/<CHANNELNAMEWITHOUT#>'.\n", STREAM_PORT);
	eiwic->output_printf(out, "You can add '?<URLTOCSSFILE>' at the end to use a\n");
	eiwic->output_printf(out, "CSS file.\n");

	eiwic->output_printf(out, "With 'http://<MYIP>:%d/say/<CHANNELNAMEWITHOUT#>/<SENTENCE>\n", STREAM_PORT);
	eiwic->output_printf(out, "a user can say something to the channel through Eiwic.\n");

	eiwic->output_printf(out, "Note: You can embed those URLs into a PHP script, to build\n");
	eiwic->output_printf(out, "a simple webchat.\n");
}

void stream_send(CHANNEL *chan, char *text);

void stream_send_history(struct _stream_client *c)
{
	int i=0, j=0;
	
	while (stream_channels[j]->chan != c->chan) ++j;
	while (i<MAX_LINES&&stream_channels[j]->lines[i])
	{
		send(c->conn->sock,
				stream_channels[j]->lines[i],
				strlen(stream_channels[j]->lines[i]),
				0);
		++i;
	}
}

int stream_read(CONN *conn, char *data, u_int data_len)
{
	char *url, *sd, sf[2000], sx[2040];
	int i=0;
	
	while(i<MAX_CLIENTS)
		if (stream_clients[i].used&&stream_clients[i].conn==conn)
			break;
		else i++;
			
	if (i==MAX_CLIENTS)
		return -1;
	
	if (stream_clients[i].chan)
		return -1;
	
	if (data_len < 10 || strncmp(data, "GET ", 4))
	{
		mlink->conn_close(conn);
		return -1;
	}
	
	if (NULL == (url = strtok(data+4, " \r\n")) || *url != '/')
	{
		mlink->conn_close(conn);
		return -1;
	}
	
	mlink->log(LOG_DEBUG, "Client asked for %s", url);

	sd = "HTTP/1.0 200 OK\r\n"
		 "Server: "NAME"/"VERSION"\r\n"
		 "Content-Type: multipart/mixed;boundary=xxThisRandomString\r\n"
		 "Pragma: no-cache\r\n"
		 "Cache-control: no-cache\r\n"
		"\r\n"
		"--xxThisRandomString\r\n"
		"Content-Type: text/html\r\n"
		"\r\n"
		;

	send(conn->sock, sd, strlen(sd), 0);

	if (strncmp(url, "/chan/", 6) == 0)
	{
	 	char *style=NULL;
		
		url += 5;
		*url = '#';
		
		if (style = strchr(url, '?'))
		{
			*style++ = '\0';
		}
		
		if ((stream_clients[i].chan = mlink->channel_find(url)) == NULL)
		{
			sprintf(sf, "<html><head><title>404 Channel not found</title></head>"
					"<body bgcolor='#8888ff'>Channel %s not found</body></html>", url);
			send(conn->sock, sf, strlen(sf), 0);
			mlink->conn_close(conn);
		}
		else
		{
			mlink->log(LOG_DEBUG, "Accepted streaming client %d", i);

			if (style)				
			sprintf(sf, "<HTML><HEAD><TITLE>"NAME" - %s stream</TITLE>"
					"<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
					"</HEAD>"
					"<BODY CLASS='stream'><B>"TITLE"</B><BR>\n\n",
					stream_clients[i].chan->name, style, stream_clients[i].chan->name);
			else
			sprintf(sf, "<HTML><HEAD><TITLE>"NAME" - %s stream</TITLE></HEAD>"
					"<BODY CLASS='stream'><B>"TITLE"</B><BR>\n\n",
					stream_clients[i].chan->name, stream_clients[i].chan->name);
			send(conn->sock, sf, strlen(sf), 0);
			
			sprintf(sf, "<script language=\"JavaScript\">\n"
						"<!--\n"
						"	function moves() {\n"
						"	window.scroll(1,500000)\n"
						"	window.setTimeout(\"moves()\", 100);\n"
						"	}\n"
						"	moves();\n"
						"//-->\n"
						"</script>\n\n");
			send(conn->sock, sf, strlen(sf), 0);
			
			sprintf(sf, "Pr&auml;sentiert von "NAME" "VERSION"<BR>\n");
			send(conn->sock, sf, strlen(sf), 0);
						
			stream_send_history(&stream_clients[i]);
		}
	}
	else
	if (strncmp(url, "/say/", 5) == 0)
	{
		char *x,*y,z[3];
		CHANNEL *chan;
		
		z[2] = 0;				
		url += 4;
		*url = '#';
		
		if ((x = strchr(url, '/')) == NULL)
			return -1;

		*x++ = '\0';

		if ((chan = mlink->channel_find(url)) == NULL)
			return -1;

		for (y = x; *x; x++)
			if (*x == '%' && !(*(x+1)==0||*(x+2)==0) && *(x+1)!='0')
			{
				strncpy(z, x + 1, 2);				
				*x++ = (unsigned char)strtol(z, NULL, 16);
				memmove(x, x + 2, strlen(x + 2) + 1);
				x--;
			}	
			
		if (conn->host == inet_addr("127.0.0.1"))	
		{
		mlink->plug_sendf("PRIVMSG %s :%s\r\n",
				chan->name,y);
		} else 
		{
		mlink->plug_sendf("PRIVMSG %s :<%s> %s\r\n",
				chan->name,
				inet_ntoa(*(struct in_addr *)&conn->host),
				y);		
}
		sprintf(sx, "<b>-&gt;</b> %s<br>\n", y);
		stream_send(chan, sx);
		mlink->conn_close(conn);
	}
	else
	{
		CHANNEL *channel;
		
		FOR_EACH_DATA(&e_global->channels, channel)
		{
			sprintf(sf, "<li><a href=\"/chan/%s\">%s</a></li>", channel->name+1,channel->name);
			send(conn->sock, sf, strlen(sf), 0);
		}
		END_DATA;
		
		sprintf(sf, "<hr size=1>Eiwic "VERSION"<BR>", channel->name+1,channel->name);
		send(conn->sock, sf, strlen(sf), 0);

		mlink->conn_close(conn);
	}
}

int stream_close(CONN *conn)
{
	int i=0;
	while(i<MAX_CLIENTS)
	{ 
		if(stream_clients[i].conn==conn&&stream_clients[i].used==1)
		{
			mlink->log(LOG_DEBUG, "Client %d disconnected.", i);
			stream_clients[i].used=0;
			stream_clients[i].chan=NULL;
		}
		++i;
	}
	return 0;
}

int stream_incoming(CONN *new_conn)
{
	int i=0;
	
	while(stream_clients[i].used&&i<MAX_CLIENTS) ++i;
		
	if (i==MAX_CLIENTS)
	{
		mlink->log(LOG_WARNING, "All connections in use, disconecting client...");
		mlink->conn_close(new_conn);
		return -1;
	}
	
	mlink->log(LOG_DEBUG, "Client %d connected.", i);
	stream_clients[i].used = 1;
	stream_clients[i].conn = new_conn;
	stream_clients[i].chan = NULL;
	return 1;
}

void stream_send(CHANNEL *chan, char *text)
{
	int i=0,j=0;

#ifdef VV_DEBUG
	mlink->log(LOG_DEBUG, "Sending \"%s\" to %s", text, chan->name);
#endif
	
	while (stream_channels[i]->chan != chan) ++i;
	while (stream_channels[i]->lines[j] != NULL && j<MAX_LINES) ++j;
		
	if (j==MAX_LINES) /* the lines are full.. so letz move it.. */
	{
		free(stream_channels[i]->lines[0]);
		memmove(&stream_channels[i]->lines[0], &stream_channels[i]->lines[1], sizeof(void *)*(MAX_LINES-1));
		stream_channels[i]->lines[MAX_LINES-1] = malloc(strlen(text)+1);
		strcpy(stream_channels[i]->lines[MAX_LINES-1], text);
	}
	else /* yea.. found a free line! inserting.. */
	{
		stream_channels[i]->lines[j] = malloc(strlen(text)+1);
		strcpy(stream_channels[i]->lines[j], text);
	}
	
	i=0;
	while(i<MAX_CLIENTS)
	{
		if (stream_clients[i].used&&stream_clients[i].chan==chan)
		{
			if (-1 == send(stream_clients[i].conn->sock, text, strlen(text), 0))
			{
				mlink->log(LOG_WARNING, "disconnecting client");
				stream_clients[i].used = 0;
				mlink->conn_close(stream_clients[i].conn);
			}
		}
		++i;
	}

#ifdef VV_DEBUG
	mlink->log(LOG_DEBUG, "done");
#endif
	
}
				
int ep_parse(MSG *ircmsg)
{
	CHANNEL *m;
	char *buf, y[1024];	u_char c1, c2, c3;
	
	if ((m = mlink->channel_find(ircmsg->to)) || (m = mlink->channel_find(ircmsg->args)))
	{
		int i=0,x;
		struct tm *t;
		time_t v;
		v = time(NULL);
			
		t = localtime(&v);	
		sprintf(y, "<I>(%2.2d:%2.2d)</I> ", t->tm_hour, t->tm_min);
		buf = y + strlen(y);
		
		*buf = '\0';
		c1 = (u_char)(*ircmsg->nick * 10);
		c2 = (u_char)(*(ircmsg->nick+1) * 100);
		c3 = (u_char)(*(ircmsg->nick+2) * 100);
		
		if (c1+c2+c3>560)
		{
			if (c1>190)c1=190;
			if (c1>190)c1=190;
			if (c1>190)c1=190;
		}
				
		if (strcmp(ircmsg->cmd, "privmsg") == 0)
		{
			if (*ircmsg->args == 1)
			{
				if (strncmp(ircmsg->args +1, "ACTION ", 7) == 0)
				sprintf(buf, "<FONT COLOR='#%2.2X%2.2X%2.2X'><B>%s</B></FONT> %s<BR>\n",
					c1, c2, c3,	ircmsg->nick, ircmsg->args+8);
				for (x=0;buf[x];x++)if(buf[x]==1)memmove(buf+x, buf+x+1,strlen(buf+x+1));
			}
			else
			{
				sprintf(buf, "(<FONT COLOR='#%2.2X%2.2X%2.2X'><B>%s</B></FONT>) %s<BR>\n",
					c1, c2, c3,	ircmsg->nick, ircmsg->args);
			}
		}
		else
		if (strcmp(ircmsg->cmd, "join") == 0)
			sprintf(buf, "* <FONT COLOR='#%2.2X%2.2X%2.2X'>%s</FONT> enters.<BR>\n",
				c1,c2,c3,ircmsg->nick);
		else
		if (strcmp(ircmsg->cmd, "part") == 0)
			sprintf(buf, "* <FONT COLOR='#%2.2X%2.2X%2.2X'>%s</FONT> leaves.<BR>\n",
				c1,c2,c3,ircmsg->nick);
		
		if (*buf)
			stream_send(m, y);
	}
}

int ep_main(OUTPUT *out)
{
	int num = 0;
	CHANNEL *chan;
	
	memset(stream_clients, 0, sizeof(stream_clients));
	
	if (!eiwic->conn_listen(STREAM_PORT, 0, stream_incoming, stream_read, NULL, stream_close))
	{
		eiwic->output_printf(out, "Could not set up a listening socket on port %d.\n",
			STREAM_PORT);
		return 0;
	}
	
	FOR_EACH_DATA(&e_global->channels, chan)
		++num;
	END_DATA;
	
	eiwic->log(LOG_DEBUG, "Setting up stream structures (%d channel[s])..", num);
	stream_channels = malloc(sizeof(struct _stream_channel *) * num);
	num = 0;

	FOR_EACH_DATA(&e_global->channels, chan)
	{
#ifdef VV_DEBUG
		eiwic->log(LOG_DEBUG, "-> %d/%s", num, chan->name);
#endif
		stream_channels[num] = calloc(1, sizeof(struct _stream_channel));
		stream_channels[num]->chan = chan;
		++num;
	}
	END_DATA;
	
	eiwic->output_printf(out, "Okay, stream module successfully set up for %d channel(s),\n", num);
	eiwic->output_printf(out, "the url is http://<MYIP>:%d/, have fun :).\n", STREAM_PORT);

	return 1;
}

