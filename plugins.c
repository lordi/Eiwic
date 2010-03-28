#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <time.h>

#include "eiwic.h"
#include "settings.h"
#include "plugins.h"


/*--- output functions ---*/

int output_route(OUTPUT *op, char *s, int len)
{
	char *buf;
	MSG msg;

#ifdef VV_DEBUG
	enter_vvd_section("output_route");
#endif

	switch (op->op_type)
	{
	case OP_DOUBLE:
#ifdef VV_DEBUG
			log(LOG_VERBOSE_DEBUG, "Routing %d--(double_forward)-->%d & -->%d: %d bytes",
				op->op_id,
				op->data.d_double.f1->op_id,
				op->data.d_double.f2->op_id,
				len);
#endif
			output_print(op->data.d_double.f1, s);
			output_print(op->data.d_double.f2, s);
		break;
	case OP_LOG:
#ifdef VV_DEBUG
			log(LOG_VERBOSE_DEBUG, "Routing %d--(log)-->loglevel=%d: %d bytes", op->op_id,
				op->data.d_loglevel, len);
#endif
			log(op->data.d_loglevel, "%s", s);
		break;
	case OP_SERVER_INPUT:
#ifdef VV_DEBUG
			log(LOG_VERBOSE_DEBUG, "Routing %d--(server_input)-->irc_parse(): %d bytes", op->op_id, len);
#endif
			irc_resolve(s, &msg);
			irc_parse(&msg);
		break;
	case OP_QUERY: 
#ifdef VV_DEBUG
			log(LOG_VERBOSE_DEBUG, "Routing %d--(query)-->%d: %d bytes", op->op_id,
				e_global->server_output->op_id, len);
#endif
			buf = malloc(len + 1);
			memcpy(buf, s, len);
			buf[len] = 0;
			output_print(e_global->server_output, "PRIVMSG ");
			output_print(e_global->server_output, op->data.d_user->nick);
			output_print(e_global->server_output, " :");
			output_print(e_global->server_output, buf);
			output_print(e_global->server_output, "\r\n");
			free(buf);
		break;
	case OP_NOTICE: 
#ifdef VV_DEBUG
			log(LOG_VERBOSE_DEBUG, "Routing %d--(notice)-->%d: %d bytes", op->op_id,
				e_global->server_output->op_id, len);
#endif
			buf = malloc(len + 1);
			memcpy(buf, s, len);
			buf[len] = 0;
			output_print(e_global->server_output, "NOTICE ");
			output_print(e_global->server_output, op->data.d_user->nick);
			output_print(e_global->server_output, " :");
			output_print(e_global->server_output, buf);
			output_print(e_global->server_output, "\r\n");
			free(buf);
		break;
	case OP_CHANNEL: 
#ifdef VV_DEBUG
			log(LOG_VERBOSE_DEBUG, "Routing %d--(channel)-->%d: %d bytes", op->op_id,
				e_global->server_output->op_id, len);
#endif
			buf = malloc(len + 1);
			memcpy(buf, s, len);
			buf[len] = 0;
			output_print(e_global->server_output, "PRIVMSG ");
			output_print(e_global->server_output, op->data.d_channel->name);
			output_print(e_global->server_output, " :");
			output_print(e_global->server_output, buf);
			output_print(e_global->server_output, "\r\n");
			free(buf);
		break;
	case OP_SERVER:
#ifdef VV_DEBUG
			log(LOG_VERBOSE_DEBUG, "Routing %d--(server)-->%s: %d bytes", op->op_id,
				e_global->server->host, len);
#endif
			send(e_global->var_socket, s, len, 0);
		break;
	}
}

int output_buffer_line(OUTPUT *op, char *s)
{
	int r,l;
	char *data,*end;

#ifdef VV_DEBUG
	log(LOG_VERBOSE_DEBUG, "output %d: okay, trying to line-buffer %15s...!", op->op_id, s);
#endif

	if (s == NULL) return 0;

#ifdef VV_DEBUG
	enter_vvd_section("output_buffer_line");
	e_global->vvd_param = s;	
#endif

	r = strlen(s);
	l = strlen(op->op_buffer);
	
#ifdef VV_DEBUG
	log(LOG_VERBOSE_DEBUG, "output %d: %p, old=%d, new=%d", op->op_id, op->op_buffer, l, r+l+1);
#endif

	if (r > 0)
	{
#ifdef VV_DEBUG
		log(LOG_VERBOSE_DEBUG, "output %d: op_buffer=%p", op->op_id, op->op_buffer);
#endif
		op->op_buffer = realloc(op->op_buffer, r + l + 1);
#ifdef VV_DEBUG
		log(LOG_VERBOSE_DEBUG, "output %d: op_buffer=%p", op->op_id, op->op_buffer);
#endif
		strcpy(op->op_buffer + l, s);
		op->op_buffer[r + l] = 0;
		data = op->op_buffer;
							 
		while ((end = strchr(data, '\n')) != NULL)
		{
			*end = 0;

			output_route(op, data, (int)(end - data));
			
			data = end + 1;			
		}
		 
		if (end == NULL && *data != 0)
		{							
			char *p = malloc(strlen(data)+1);
			strcpy(p, data);
#ifdef VV_DEBUG
			enter_vvd_section("output_buffer_line:buffer_left");
#endif
#ifdef VV_DEBUG
			log(LOG_VERBOSE_DEBUG, "line-buffer-left: %d (%4s..) [%p]", strlen(data),data,op->op_buffer);
#endif
			op->op_buffer = realloc(op->op_buffer, strlen(data) + 1);
			strcpy(op->op_buffer, p);
			free(p);
		}
		else
		{
#ifdef VV_DEBUG
			enter_vvd_section("output_buffer_line:end_of_buffer");
#endif
			op->op_buffer = realloc(op->op_buffer, 1);
			*op->op_buffer = '\0';
		}
	}
#ifdef VV_DEBUG
	enter_vvd_section("output_buffer_line:end");
#endif
	return 1;
}

int output_buffer_server(OUTPUT *op, char *s)
{
	u_int count;

#ifdef VV_DEBUG
	enter_vvd_section("output_buffer_server");
	e_global->vvd_param = s;
#endif

	if (e_global->var_noconnect == 1)
		return 1;
	
	if (s)
	{
		if (op->op_buffer)
		{
#ifdef VV_DEBUG
			log(LOG_DEBUG, "output %d: reallocating buffer to %i bytes.",
				op->op_id,
				strlen(op->op_buffer) + strlen(s) + 1);
#endif
			op->op_buffer = realloc(op->op_buffer, strlen(op->op_buffer) + strlen(s) + 1);
			strcat(op->op_buffer, s);
		}
		else
		{
#ifdef VV_DEBUG
			log(LOG_DEBUG, "output %d: allocating buffer of %i bytes.",
				op->op_id,
				strlen(s) + 1);
#endif
			op->op_buffer = malloc(strlen(s) + 1);
			strcpy(op->op_buffer, s);			
		}
	}

#ifdef VV_DEBUG
	enter_vvd_section("output_buffer_server:chk2");
#endif


	if (op->op_buffer == NULL) return 0;
		
	if (e_global->var_bytes >= 200)
		return 0;
	else	
		count = 200 - e_global->var_bytes;
	
	
	if (s == NULL)
	{

	if (strlen(op->op_buffer) <= count)
	{
#ifdef VV_DEBUG
		enter_vvd_section("output_buffer_server:chk_complete");
#endif

#ifdef VV_DEBUG
		log(LOG_DEBUG, "output %d: sent complete buffer of %i bytes.",
			op->op_id,
			strlen(op->op_buffer));
#endif

		output_route(op, op->op_buffer, strlen(op->op_buffer));
		e_global->var_bytes += strlen(op->op_buffer);
		free(op->op_buffer);
		op->op_buffer = NULL;
	}
	else
	{
#ifdef VV_DEBUG
		enter_vvd_section("output_buffer_server:chk_part");
#endif

#ifdef VV_DEBUG
		log(LOG_DEBUG, "output %d: sent %d of %d buffered bytes.",
			op->op_id,
			count,
			strlen(op->op_buffer));
#endif
		output_route(op, op->op_buffer, count);
		e_global->var_bytes += count;
		memcpy(op->op_buffer, op->op_buffer + count, strlen(op->op_buffer) - count);
		op->op_buffer[strlen(op->op_buffer) - count] = 0;
		op->op_buffer = realloc(op->op_buffer, strlen(op->op_buffer) + 1);
	}

	}
}

char *output_tostring(OUTPUT *op) {
	static char buf[50];
	/* TODO: fix %s to limit length */
	switch (op->op_type)
	{
	case OP_EAT: 
			sprintf(buf, "eaten");
		break;
	case OP_LOG: 
			sprintf(buf, "log (level %d)", 
				op->data.d_loglevel);
		break;
	case OP_QUERY: 
	case OP_NOTICE:
			snprintf(buf, sizeof(buf), "user %s", op->data.d_user->nick);
		break;
	case OP_CHANNEL: 
			snprintf(buf, sizeof(buf), "channel %s", op->data.d_channel->name);
		break;
	case OP_SERVER:
	       		sprintf(buf, "server");	
		break;
	default:
			sprintf(buf, "output type %d", op->op_type);
	}
	return (char *)buf;	
}

OUTPUT *output_open(int op_type, ...)
{
	va_list ap;
	OUTPUT *op;

	op = malloc(sizeof(OUTPUT));
	memset(op, 0, sizeof(OUTPUT));
	op->op_id = ++e_global->var_highest_output;
	op->op_type = op_type;
	va_start(ap, op_type);	
	switch (op->op_type)
	{
	case OP_EAT: 
			op->op_buffered = OB_NONE;
		break;
	case OP_LOG: 
			op->op_buffered = OB_LINE;
			op->data.d_loglevel = va_arg(ap, int);
		break;
	case OP_SERVER_INPUT:
			op->op_buffered = OB_LINE;
		break;
	case OP_QUERY: 
	case OP_NOTICE:
			op->op_buffered = OB_LINE;
			op->data.d_user = va_arg(ap, USER *);
		break;
	case OP_CHANNEL: 
			op->op_buffered = OB_LINE;
			op->data.d_channel = va_arg(ap, CHANNEL *);
		break;
	case OP_SERVER: 
			op->op_buffered = OB_SERVER;
			op->data.d_server = va_arg(ap, SERVER *);
		break;
	case OP_FILE: 
			op->op_buffered = OB_NONE;
			/* TODO */
		break;
	case OP_FORWARD: 
			op->op_buffered = OB_NONE;
			op->data.d_output = va_arg(ap, OUTPUT *);
		break;
	case OP_DOUBLE: 
			op->op_buffered = OB_NONE;
			op->data.d_double.f1 = va_arg(ap, OUTPUT *);
			op->data.d_double.f2 = va_arg(ap, OUTPUT *);
		break;
	default:
			free(op);
			va_end(ap);
		return NULL;
	}
	va_end(ap);
	switch (op->op_buffered)
	{
	case OB_LINE:
		op->op_buffer = malloc(1);
		op->op_buffer[0] = '\0';
		break;
	default:
		op->op_buffer = NULL;
	}

	/* TODO: add output to e_global->outputs */

	return op;
	
}

int output_printf(OUTPUT *op, char *fmt, ...)
{
	va_list m;
	char buf[1024];

	if (fmt == NULL)
		return output_print(op, NULL);
	
	va_start(m, fmt);
	vsprintf(buf, fmt, m);
	va_end(m);
	
	return output_print(op, buf);
}

int output_print(OUTPUT *op, char *s)
{
#ifdef VV_DEBUG
	enter_vvd_section("output_print");
#endif

	/* TODO: test if output is still in e_global->outputs still exist */

#ifdef VV_DEBUG
	if (s)
		log(LOG_VERBOSE_DEBUG, "output %d: about to buffer '%20s'", op->op_id, s);
#endif

	switch (op->op_buffered)
	{
	case OB_SERVER:
		output_buffer_server(op, s);
		break;
	case OB_LINE:
		output_buffer_line(op, s);
		break;
	case OB_NONE:
	default:
		if (s != NULL) output_route(op, s, strlen(s));
		break;
	}

#ifdef VV_DEBUG
	enter_vvd_section(NULL);
#endif

	return 1;
}

int output_close(OUTPUT *op)
{
	log(LOG_DEBUG, "output: closing output %d.", op->op_id);
}

void output_destroy(void *data)
{
	OUTPUT *output = data;

	free(output);
}






	
/*--- settings functions ---*/

SETTING *set_find(char *label)
{
	SETTING *set;
	FOR_EACH_DATA(&e_global->settings, set)
	{
		if (strcasecmp(set->label, label) == 0)
		{
			return set;
		}
	}
	END_DATA;

	return NULL;
}

char *set_get_string(char *label)
{
	SETTING *s = set_find(label);

	if (s)
	switch(s->type)
	{
	case SET_STRING:
	case SET_LONGSTRING:
		return s->data.d_string;
	}

	return "";
}


long set_get_int(char *label)
{
	SETTING *s = set_find(label);

	if (s)
	switch(s->type)
	{
	case SET_INT:
		return s->data.d_int;
	}

	return 0;
}

float set_get_float(char *label)
{
	SETTING *s = set_find(label);

	if (s)
	switch(s->type)
	{
	case SET_FLOAT:
		return s->data.d_float;
	}

	return 0;
}

int set_is(char *label)
{
	SETTING *s = set_find(label);

	if (s)
	switch(s->type)
	{
	case SET_BOOL:
		return s->data.d_int;
	case SET_INT:
		return ((s->data.d_int > 0)?1:0);
	case SET_STRING:
	case SET_LONGSTRING:
		return ((strlen(s->data.d_string) > 0)?1:0);
	}

	return 0;
}

void set_set(SETTING *s, ...)
{
	va_list ap;
	char *x;

	va_start(ap, s);	
	switch (s->type)
	{
	case SET_BOOL:
	case SET_INT:
			s->data.d_int = va_arg(ap, long);
#ifdef VV_DEBUG
			log(LOG_INIT_DEBUG, "Setting '%s' changed to '%i'",
					s->label, s->data.d_int);
#endif
		break;
	case SET_FLOAT:
			s->data.d_float = va_arg(ap, double);
#ifdef VV_DEBUG
			log(LOG_INIT_DEBUG, "Setting '%s' changed to '%2.2f'",
					s->label, s->data.d_float);
#endif
		break;
	case SET_STRING:
	case SET_LONGSTRING:
			x = va_arg(ap, char *);
			if (s->type == SET_STRING)
			{
				s->data.d_string = realloc(s->data.d_string, MIN(strlen(x), 128) + 1);
				strncpy(s->data.d_string, x, MIN(strlen(x), 128));
				s->data.d_string[MIN(strlen(x), 128)] = 0;
			}
			else
			{
				s->data.d_string = realloc(s->data.d_string, strlen(x) + 1);
				strcpy(s->data.d_string, x);
			}
#ifdef VV_DEBUG
			log(LOG_INIT_DEBUG, "Setting '%s' changed to '%s'",
					s->label, s->data.d_string);
#endif
		break;
	}
	va_end(ap);
}

SETTING *set_add(int type, char *label, ...)
{
	va_list ap;
	SETTING *s;

	if (type < SET_BOOL || type > SET_LONGSTRING)
	{
		log(LOG_DEBUG, "Couldn't add setting: Unknown type: %i", type);
		return NULL;
	}

	s = malloc(sizeof(SETTING));
	strncpy(s->label, label, 63);
	s->type = type;
	s->data.d_string = NULL;
	s->data.d_int = 0;
	s->data.d_float = 0.0;

	va_start(ap, label);
	set_set(s, va_arg(ap, void*));
	va_end(ap);
	
	dlist_ins_next(&e_global->settings, dlist_tail(&e_global->settings), s);
	
	log(LOG_INIT_DEBUG, "Setting '%s' (type %i) has been added.", s->label, s->type);

	return s;
}

void set_remove(SETTING *set)
{
	return;
}

void set_destroy(void *s)
{
	SETTING *set = s;

	if (set->type == SET_STRING ||
			set->type == SET_LONGSTRING)
	{
		free(set->data.d_string);
	}

	free(s);
}

/*--- end of settings functions ---*/



void log(int verbose_level, char *line, ...)
{
	va_list m;
	FILE *out;

	if (e_global->var_log == 0)
	{
		if (verbose_level <= LOG_WARNING)
			out = stderr;
		else
			return;
	}
	else
		out = e_global->var_logfile;

	if (verbose_level != LOG_PLUGIN_OUTPUT)
	{
		if (e_global->var_verbose <= verbose_level)
			return;
		
		if (plugin)
		{
			fprintf(out, "%s: ", plugin->name);
		}
		else
		{
			fprintf(out, "eiwic: ");
		}
		
		switch (verbose_level)
		{
			case LOG_PLUGIN_OUTPUT:
				break;
			case LOG_ERROR:
					fprintf(out, "Error: ");
				break;
			case LOG_WARNING:
					fprintf(out, "Warning: ");
				break;
			case LOG_STATUS:
			case LOG_DEBUG:
#ifdef VV_DEBUG
			case LOG_VERBOSE_DEBUG:
#endif
			case LOG_MISC:
				break;
		}
	
	}
	va_start(m, line);
	vfprintf(out, line, m);
	va_end(m);
	
	if (errno && (verbose_level == LOG_ERROR || verbose_level == LOG_WARNING))
	{
		fprintf(out, " (%s)", strerror(errno));
	}
	
	fprintf(out, "\n");
	fflush(out);
}

void plog(PLUGIN *plugin, int verbose_level, char *line, ...)
{

	va_list m;
	FILE *out;
	
	if (e_global->var_log == 0)
	{
		if (verbose_level <= LOG_WARNING)
			out = stderr;
		else
			return;
	}
	else
		out = e_global->var_logfile;
	
	if (e_global->var_verbose <= verbose_level)
		return;
	
	if (plugin)
	{
		fprintf(out, "%s: ", plugin->name);
	}
	else
	{
		fprintf(out, "eiwic: ");
	}
	
	switch (verbose_level)
	{
		case LOG_ERROR:
				fprintf(out, "Error: ");
			break;
		case LOG_WARNING:
				fprintf(out, "Warning: ");
			break;
		case LOG_STATUS:
		case LOG_DEBUG:
#ifdef VV_DEBUG
		case LOG_VERBOSE_DEBUG:
#endif
		case LOG_MISC:
			break;
	}
	
	va_start(m, line);
	vfprintf(out, line, m);
	va_end(m);
	
	if (errno && verbose_level <= LOG_WARNING)
	{
		fprintf(out, " (%s)", strerror(errno));
	}
	
	fprintf(out, "\n");
	fflush(out);
}




/*--- channel functions ---*/

CHANNEL *channel_add(char *name)
{
	CHANNEL *chan;

	chan = channel_find(name);

	if (chan != NULL) return chan;

	chan = malloc(sizeof(CHANNEL));
	bzero(chan, sizeof(CHANNEL));

	strncpy(chan->name, name, sizeof(chan->name) - 1);
	chan->output = output_open(OP_CHANNEL, chan);

	dlist_ins_next(&e_global->channels, dlist_tail(&e_global->channels), chan);
#ifdef VV_DEBUG
	log(LOG_VERBOSE_DEBUG, "Channel %s has been added (output id is %d).", chan->name, chan->output->op_id);
#endif

	return chan;
}

void channel_update(void)
{
	DListElmt *el;
	
	FOR_EACH(&e_global->channels, el)
	{
		channel_join(el->data);
	}
}

void channel_remove(CHANNEL *chan)
{
	return;
}

void channel_destroy(void *data)
{
	CHANNEL *chan = data;

	free(chan);
}

void channel_join(void *which)
{
	CHANNEL *chan = which;
	
	if (chan == NULL)
		return;
	
#ifdef VV_DEBUG
	log(LOG_VERBOSE_DEBUG, "Attempting to join %s", chan->name);
#endif
	plug_sendf("JOIN %s\r\n", chan->name);
}

CHANNEL *channel_find(char *name)
{
	DListElmt *el;

	FOR_EACH(&e_global->channels, el)
		if (strcasecmp(((CHANNEL *)el->data)->name, name) == 0)
			return (CHANNEL *)el->data;
		
	return NULL;
}

/*--- end of channel functions ---*/











/*--- plugin functions ---*/

PLUGIN *plug_load(char *filename, OUTPUT *out)
{
	PLUGIN *n=NULL;
	void *handle;
	char fn[1024],i=0,*modpath;
	char *(*ep_getname)(), *name;
  struct stat buf;
  
  

	modpath = set_get_string("module_path");
	do {
		if (i > 0)
		{
		  log(LOG_WARNING, "Error: %d %s", stat(fn, &buf), dlerror());
		  if (stat(fn, &buf) != -1)
		  	return NULL;
		}
			
		switch (i)
		{
		case 0: sprintf(fn, "%s%s",modpath,filename); break;
		case 1: sprintf(fn, "%s%s.so",modpath,filename); break;
		case 2: sprintf(fn, "%s%s.dll",modpath,filename); break;
		case 3:	sprintf(fn, "%s", filename); break; 
		case 4:	return NULL;
   	}
	
		log(LOG_INIT_DEBUG, "plug_load: Trying %s", fn);

#ifdef __OpenBSD__
		handle = dlopen(fn, DL_LAZY);
#else
		handle = dlopen(fn, RTLD_NOW);
#endif

		i++;

	} while (handle == NULL); 

#ifdef __OpenBSD__
	ep_getname = dlsym(handle, "_ep_getname");
#else
	ep_getname = dlsym(handle, "ep_getname");
#endif
	
	if (ep_getname == NULL)
	{	
		log(LOG_ERROR, "Failed to find routine 'ep_getname' from module %s", filename);
		dlclose(handle);
		return NULL;
	}

	if (
#ifdef __OpenBSD__
			dlsym(handle, "_ep_init")
#else
			dlsym(handle, "ep_init")
#endif
			== NULL)
	{
		log(LOG_ERROR, "Failed to find routine 'ep_init' from module %s", filename);
		dlclose(handle);
		return NULL;
	}

	name = ep_getname();
	
	if (name == NULL)
	{	
		log(LOG_ERROR, "Failed to init module %s through routine 'ep_getname'", filename);
		dlclose(handle);
		return NULL;
	}
	else
	{
		FOR_EACH_DATA(&e_global->plugins, n)
		{
			if (strcmp(name, n->name) == 0) break;
			else n = NULL;
		}
		END_DATA;

		if (n != NULL)
		{
			log(LOG_ERROR, "Failed to load module %s: It is already loaded.", filename);
			dlclose(handle);
			return NULL;
		}

		n = malloc(sizeof(PLUGIN));
		bzero(n, sizeof(PLUGIN));
			
		n->handle = handle;
		n->modlink = e_global->modlink;

		n->init_out = out;
				
		strcpy(n->name, name);
		strcpy(n->filename, fn);

#ifdef __OpenBSD__
		n->ep_init = dlsym(handle, "_ep_init");
		n->ep_main = dlsym(handle, "_ep_main");
		n->ep_unload = dlsym(handle, "_ep_unload");
		n->ep_parse = dlsym(handle, "_ep_parse");
		n->ep_trigger = dlsym(handle, "_ep_trigger");
		n->ep_help = dlsym(handle, "_ep_help");
#else
		n->ep_init = dlsym(handle, "ep_init");
		n->ep_main = dlsym(handle, "ep_main");
		n->ep_unload = dlsym(handle, "ep_unload");
		n->ep_parse = dlsym(handle, "ep_parse");
		n->ep_trigger = dlsym(handle, "ep_trigger");
		n->ep_help = dlsym(handle, "ep_help");
#endif

		plug_add(n);

		e_global->mod_need_init = 1;
	}
	
	return n;
}

int plug_unload(PLUGIN *ep)
{
	TRIGGER *trig;

trigger_del:
	FOR_EACH_DATA(&e_global->triggers, trig)
		if (trig->plugin == ep)
		{
			plug_trigger_unreg(trig);
			goto trigger_del;
		}
	END_DATA;
	
	plug_remove(ep);
	plug_destroy(ep);

	return 1;
}

void plug_add(PLUGIN *ep)
{
	dlist_ins_next(&e_global->plugins, dlist_tail(&e_global->plugins), ep);
	log(LOG_INIT_DEBUG, "Plugin %s has been added.", ep->name);
#ifdef VV_DEBUG
	log(LOG_VERBOSE_DEBUG, "--> name: %s; ep_init: %i; handle: %i; filename: %s;", ep->name, ep->ep_init, ep->handle,  ep->filename);
#endif
	
}

void plug_remove(PLUGIN *ep)
{
	dlist_remove(&e_global->plugins, dlist_find(&e_global->plugins, ep), NULL);	
}

void plug_destroy(void *data)
{
	PLUGIN *ep = data;
	
	dlclose(ep->handle);
	free(ep);
}

/*--- end of plugins functions ---*/






/*--- trigger functions ---*/

TRIGGER *plug_trigger_reg(char *trigger, u_int flags, e_trigger_f func)
{
	TRIGGER *ept;
		
	ept = malloc(sizeof(struct eiwic_plugin_trigger));
	bzero(ept, sizeof(struct eiwic_plugin_trigger));
		
	strcpy(ept->trigger, trigger);
	ept->func = func;
	ept->plugin = plugin;
	ept->flags = flags;

	log(LOG_INIT_DEBUG, "Added trigger: %s (plugin %p)", trigger, plugin);
	
	dlist_ins_next(&e_global->triggers, e_global->triggers.tail, ept);

	return ept;
}

int plug_trigger_unreg(TRIGGER *trigger)
{
	dlist_remove(&e_global->triggers, dlist_find(&e_global->triggers, trigger), NULL);
	plug_trigger_destroy(trigger);
	
	return 1;
}

void plug_trigger_destroy(void *data)
{
	TRIGGER *ept = data;
	free(ept);
}

/*--- end of trigger functions ---*/








/*--- timer functions ---*/

TIMER *plug_timer_reg(u_long ut, e_timer_f func, void *data)
{
	TIMER *ept;
	
	ept = malloc(sizeof(TIMER));
	bzero(ept, sizeof(TIMER));
		
	ept->time = ut;
	ept->func = func;
	ept->plugin = plugin;
	ept->data = data;
	
	dlist_ins_next(&e_global->timers, e_global->timers.tail, ept);

	return ept;
}

int plug_timer_unreg(TIMER *timer)
{
	dlist_remove(&e_global->timers, dlist_find(&e_global->timers, timer), NULL);	
	plug_timer_destroy(timer);
	return 0;
}

void plug_timer_destroy(void *data)
{
	TIMER *timer = data;
	free(timer);
}

int plug_timers(void)
{
	TIMER *t;
	PLUGIN *p;
	e_timer_f on_timer;
	void *data;
	DListElmt *el;
	
start:
	FOR_EACH(&e_global->timers, el)
	{
		t = el->data;
		
		if (t->time < time(NULL))
		{
			data = t->data;
			on_timer = t->func;
			p = t->plugin;
			plug_timer_unreg(t);
			
			e_global->jump_back = 1;
			if (sigsetjmp(e_global->jmp_past, 1) == 0)
			{
				plugin = p;
				on_timer(data);
				plugin = NULL;
			}
			else
			{
				e_global->jump_back = 0;
				SEGWARN(p, "e_timer_f");
			}
			e_global->jump_back = 0;
			
			goto start;
		}
	}
	
	return 1;
}

/*--- end of timer functions ---*/

void strtolower(char *p)
{
	for(;*p;++p) *p = tolower(*p);
}

void strtoupper(char *p)
{
	for(;*p;++p) *p = toupper(*p);
}

int findip(char *hostname, struct in_addr *addr)
{
	struct hostent *hent;

	hent = gethostbyname(hostname);
	if (hent == NULL)
		return -1;
	
	bzero(addr, sizeof(struct in_addr));
	memcpy(addr, hent->h_addr, hent->h_length);
	
	if ((addr->s_addr == INADDR_ANY) ||
		(addr->s_addr == INADDR_NONE))
		addr->s_addr = inet_addr(hostname);
	
	if ((addr->s_addr == INADDR_ANY) ||
		(addr->s_addr == INADDR_NONE))
		return -1;
	else
		return 1;
}

#ifdef IPV6
int findip6(char *hostname, struct in6_addr *addr)
{
	struct hostent *hent;

	hent = gethostbyname2(hostname, AF_INET6);
	if (hent == NULL)
		return -1;
	
	bzero(addr, sizeof(struct in6_addr));
	memcpy(addr, hent->h_addr, hent->h_length);
	
	return 1;
}

char *inet6_ntoa(struct in6_addr in6)
{
	static char static_buffer[sizeof("1234:5678:9abc:def0:1234:5678:9abc:def0")];
	char *buffer;
	uint8_t *p;

	buffer = static_buffer;
	p = (uint8_t *)&in6;
	
	sprintf(buffer, "%02x%02x:%02x%02x:%02x%02x:%02x%02x:"
					"%02x%02x:%02x%02x:%02x%02x:%02x%02x",
					p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
					p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
	
	return buffer;
};
#endif 
	
int plug_sendf(char *fmt, ...)
{
	va_list m;
	char buf[1024];

	if (fmt == NULL)
		return plug_send(NULL);
	
	va_start(m, fmt);
	vsprintf(buf, fmt, m);
	va_end(m);
	
	return plug_send(buf);
}

int plug_send(char *s)
{
	u_int count;


	return output_print(e_global->server_output, s);
/*
	if (e_global->var_noconnect == 1)
		return 1;
	
	if (s)
	{
		if (e_global->b_send)
		{
//			printf("Send buffer realloced to %i bytes..", strlen(e_global->b_send) + strlen(s) + 1);
			//fflush(stdout);
			e_global->b_send = realloc(e_global->b_send, strlen(e_global->b_send) + strlen(s) + 1);
			strcat(e_global->b_send, s);
		}
		else
		{
			//printf("Send buffer allocated, %i bytes..", strlen(s) + 1);
			//fflush(stdout);
			e_global->b_send = malloc(strlen(s) + 1);
			strcpy(e_global->b_send, s);			
		}
	}

	if (e_global->b_send == NULL) return 0;
		
	if (e_global->var_bytes >= 200)
		return 0;
	else	
		count = 200 - e_global->var_bytes;
		
	if (strlen(e_global->b_send) <= count)
	{
	/*	printf("Send buffer freed, sent %i bytes. %s\n", strlen(e_global->b_send), e_global->b_send);
		fflush(stdout);*/
	/*	log(LOG_DEBUG, "=> %s", e_global->b_send); *
		send(e_global->var_socket, e_global->b_send, strlen(e_global->b_send), 0);
		
		e_global->var_bytes += strlen(e_global->b_send);
		
		free(e_global->b_send);
		e_global->b_send = NULL;
	}
	else
	{
		/*printf("_Send buffer sent, %i bytes %i\n", count, e_global->b_send);
		fflush(stdout);*/
	/*	log(LOG_DEBUG, "=> [%d]%s", count, e_global->b_send); *
		send(e_global->var_socket, e_global->b_send, count, 0);
		
		e_global->var_bytes += count;
		
		memcpy(e_global->b_send, e_global->b_send + count, strlen(e_global->b_send) - count);
		e_global->b_send[strlen(e_global->b_send) - count] = 0;
		e_global->b_send = realloc(e_global->b_send, strlen(e_global->b_send) + 1);
	}
	
	return 1;*/
}
