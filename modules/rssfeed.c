/**
 * RSS newsfeed module using libraptor 
 * 
 * @author Hannes Graeuler <lordi@styleliga.org>
 * @version 1.0
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <raptor.h>

#include "eiwic.h"
#include "plugins.h"

#define MODULE_NAME "rssfeed"
#define TITLE_LEN 512
#define URL_LEN 512
#define RAPTOR_PARSER_NAME1 "rdfxml"
#define RAPTOR_PARSER_NAME2 "rss-tag-soup"
#define ITEMS_REMEMBER 50
#define WAIT_MINUTES 30 /* re-query rss every 30 minutes */

#define CMD_ADD "!addfeed"
#define CMD_ADD_USAGE "usage: " CMD_ADD " http://examplehost/examplefeed.rdf"
#define CMD_LIST "!listfeeds"
#define CMD_LIST_USAGE "usage: " CMD_LIST
#define CMD_REMOVE "!removefeed"
#define CMD_REMOVE_USAGE "usage: " CMD_REMOVE " <index>"

#define OUTPUT_FMT "\002%s\002: %s (%s)\n"
#define PARSE_TYPE_NONE 0
#define PARSE_TYPE_CHANNEL 1
#define PARSE_TYPE_ITEM 2

EP_SETNAME(MODULE_NAME);

struct _rss_feed {
	raptor_parser *parser;
	
	/* feed information */
	char *url[URL_LEN + 1];
	char *channel_title[TITLE_LEN + 1];
	char *channel_url[URL_LEN + 1];
	
	/* last parsed item */
	char *item_title[TITLE_LEN + 1];
	char *item_url[URL_LEN + 1];
	
	/* eiwic interfaces */
	OUTPUT *out;
	CONN *conn;
	TIMER *timer;
	
	/* item hash table */
	long item_hashes[ITEMS_REMEMBER];
	int item_hashes_inc;
	
	int type;
	int announced;
	int second_chance;
};

typedef struct _rss_feed rss_feed;

rss_feed **feeds; // (void *)0-terminated list

void rss_retrieve(rss_feed *);

void rss_raptor_error_handler(rss_feed *feed, raptor_locator* locator, 
		const char *message) {
 	eiwic->log(LOG_DEBUG, "feed %p: %s", feed, message); 
}

int rss_valid_feed(rss_feed *feed) {
#ifdef VV_DEBUG
	eiwic->log(LOG_VERBOSE_DEBUG, "Is feed %p valid?",feed);
#endif
	rss_feed **p = feeds;
	do {
#ifdef VV_DEBUG
		eiwic->log(LOG_DEBUG, "Is feed %p.%p==%p?",p,*p,feed);
#endif	
		if (*p == feed) return 1; 
	} while (*(++p) != NULL);

#ifdef VV_DEBUG
	eiwic->log(LOG_DEBUG, "NOT feed valid!");
#endif 
	return 0;
}

void rss_remove_feed(rss_feed *feed) {
	int i = 0, num;
	if (!rss_valid_feed(feed)) return;
	for (num = 0; feeds[num] != NULL; num++)
		if (feeds[num] == feed) i = num;
	memmove(&feeds[i], &feeds[i + 1], (num - i) * sizeof(rss_feed *));
	feeds = realloc(feeds, num * sizeof(rss_feed *));

	if (feed->parser) raptor_free_parser(feed->parser);
	free(feed);
}

long rss_item_hash(char *str1, char *str2) {
	char *c;
	short s1 = 0, s2 = 0x1337, m = 0;
	for (c = str1; *c != '\0'; c++, m++) s1 += (*c*m);
	for (c = str2; *c != '\0'; c++) s2 ^= *c;
	
	return ((long)s1) << 4 + s2;
}

void rss_print_current_item(rss_feed *feed) {
	long hash;
	int i;
	
	hash = rss_item_hash((char *)feed->item_title, (char *)feed->item_url);
	
	for (i = 0; i < ITEMS_REMEMBER; i++) 
		if (feed->item_hashes[i] == hash) return;

	/* okay, I don't know that hash yet.. */
	
	feed->item_hashes_inc = ++(feed->item_hashes_inc) % ITEMS_REMEMBER;
	
	feed->item_hashes[feed->item_hashes_inc] = hash;
	
	if (feed->announced == 1) {
		eiwic->output_printf(feed->out, OUTPUT_FMT,
			feed->channel_title,
			feed->item_title,
			feed->item_url);
	}	
}

void rss_statement(void *data, const raptor_statement *statement) {
	struct _rss_feed *feed = data;	
	char *predicate, *object;
	
	predicate = raptor_statement_part_as_string(
			statement->predicate, 
			statement->predicate_type, NULL, NULL);
	object = raptor_statement_part_as_string(
			statement->object, statement->
			object_type, NULL, NULL);
	
	/* strip quotes */
	if (object[strlen(object) - 1] == '\"') 
		object[strlen(object) - 1] = '\0';
	if (*object == '\"') 
		object++;
	
	if (strstr(predicate, "type") && strstr(object, "channel>")) {
		feed->type = PARSE_TYPE_CHANNEL;
	}
	else if (strstr(predicate, "type") && strstr(object, "item>")) {
		if (feed->type == PARSE_TYPE_ITEM) {
			rss_print_current_item(feed);
		}
		
		feed->type = PARSE_TYPE_ITEM;
	}
	else if (strstr(predicate, "title>")) {
		char *dest = (char *)
			(feed->type == PARSE_TYPE_CHANNEL ? 
			 	feed->channel_title : feed->item_title);
		strncpy(dest, object, TITLE_LEN);
		dest[TITLE_LEN] = '\0';
	}
	else if (strstr(predicate, "link>")) {
		char *dest = 
			(char *)(feed->type == PARSE_TYPE_CHANNEL ? 
				 feed->channel_url : feed->item_url);
		strncpy(dest, object, URL_LEN);
		dest[URL_LEN] = '\0';
	}
}

raptor_parser *rss_setup_parser(struct _rss_feed *feed) {
	raptor_parser *p = raptor_new_parser(feed->second_chance ?
			RAPTOR_PARSER_NAME2 :
			RAPTOR_PARSER_NAME1);
	eiwic->log(LOG_DEBUG, "create parser=%p (%s)", p, raptor_get_name(p));
	raptor_set_parser_strict(p, 0);
	raptor_set_feature(p, RAPTOR_FEATURE_ASSUME_IS_RDF, 1);
	raptor_set_feature(p, RAPTOR_FEATURE_ALLOW_NON_NS_ATTRIBUTES, 1);
	raptor_set_feature(p, RAPTOR_FEATURE_WARN_OTHER_PARSETYPES, 0);
	raptor_set_statement_handler(p, feed, rss_statement);
	
	raptor_set_fatal_error_handler(p, feed, 
			(raptor_message_handler)rss_raptor_error_handler);
	raptor_set_error_handler(p, feed, 
			(raptor_message_handler)rss_raptor_error_handler);
	raptor_set_warning_handler(p, feed,
			(raptor_message_handler)rss_raptor_error_handler);
	
	return p;
}

int rss_arrived(char *rdf, void *data) {
	struct _rss_feed *feed = data;
	
	if (!rss_valid_feed(feed)) return 0;
	
	if (rdf == NULL) {
		eiwic->output_printf(feed->out, 
			"rssfeed: Couldn't access file, aborting.\n");
		rss_remove_feed(feed);
		return 0;
	}
	
	*feed->item_title = '\0'; /* reset title string */

	if (feed->parser == NULL) {	
		feed->parser = rss_setup_parser(feed);
	}	
	
	if (feed->parser == NULL) {	
		eiwic->output_printf(feed->out, "rssfeed: Wasn't able to create a "
			"parsing object.\n");
		rss_remove_feed(feed);
		return 0;
	}
	

#ifdef VV_DEBUG
	eiwic->log(LOG_VERBOSE_DEBUG, "Start RSS parsing. %p", feed->parser);
#endif
	raptor_start_parse(feed->parser, NULL);// raptor_new_uri((const unsigned char *)feed->url));
	raptor_parse_chunk(feed->parser, (const unsigned char *)rdf,
		(size_t)strlen(rdf), 1);
	rss_print_current_item(feed); /* print the last item, also */
	
#ifdef VV_DEBUG

	eiwic->log(LOG_VERBOSE_DEBUG, "End RSS parsing. %p", feed->parser);
#endif

	raptor_free_parser(feed->parser);
	feed->parser = NULL;
	
	if (strlen((char *)feed->item_title) == 0) {
		eiwic->log(LOG_WARNING, 
			"parser (%p) in feed %s failed (%d): %.6s",
			feed->parser,
			(char *)feed->url,
			feed->second_chance,
			rdf);
		if (feed->second_chance == 0) {
		//	raptor_free_parser(feed->parser);
			feed->parser = NULL;
			feed->second_chance = 1;
			return rss_arrived(rdf, data);
		}
		else {
			eiwic->output_printf(feed->out, 
				"rssfeed: Wasn't able to parse the page.\n");
			rss_remove_feed(feed);
			return 0;
		}
	}


	if (feed->announced == 0) {
		eiwic->output_printf(feed->out, "Okay, now streaming: \002%s\002 (%s)\n",
			feed->channel_title,
			feed->channel_url);
		feed->announced = 1;
	}
	
	feed->timer = 
		eiwic->plug_timer_reg(time(NULL) + WAIT_MINUTES * 60, 
			(e_timer_f)rss_retrieve, feed);
	
	return 1;
}
	
void rss_retrieve(struct _rss_feed *f) {
	if (!rss_valid_feed(f)) return;
	f->conn = eiwic->conn_http_get((char *)f->url, rss_arrived, f);
}

int rss_add(char *param, OUTPUT *out, MSG *msg) {
	rss_feed *newfeed;
	int num = 0;
	
	if (param == NULL
		|| strncmp(param, "http://", 6) != 0 
		|| strlen(param) > URL_LEN)	{
		eiwic->output_printf(out, CMD_ADD_USAGE "\n");
		return 0;
	}
	
	newfeed = malloc(sizeof(rss_feed));
	memset(newfeed, 0, sizeof(rss_feed));
	strcpy((char *)newfeed->url, param);
	newfeed->out = out;
	
	while (feeds[num] != NULL) num++;

	num += 2;
	
	feeds = realloc(feeds, num * sizeof(rss_feed *));
	feeds[num - 2] = newfeed;
	feeds[num - 1] = NULL;
	
	rss_retrieve(newfeed);
	return 1;
}

int rss_list(char *param, OUTPUT *out, MSG *msg) {
	rss_feed *feed;
	int index = 0;

	while (feeds[index] != NULL) {
		feed = feeds[index];
		eiwic->output_printf(out, "%2d. \002%s\002 to %s (%s)\n",
			++index,
			(char *)feed->channel_title,
			eiwic->output_tostring(feed->out),
			(char *)(feed->parser == NULL ?
			"No Parser" :
			raptor_get_name(feed->parser))
			);	
	}
}

int rss_remove(char *param, OUTPUT *out, MSG *msg) {
	int index = param?atoi(param):-1;
	int num = 0;

	while (feeds[num] != NULL) num++;
	
	if (index < 1 || index > num) {
		eiwic->output_print(out, CMD_REMOVE_USAGE "\n");
		return 0;
	}

	index--;

	eiwic->output_printf(out, "rssfeed: No longer casting %s.\n",
		feeds[index]->url);	
	rss_remove_feed(feeds[index]);
	
	return 1;
}

int ep_help(OUTPUT *out) {
	eiwic->output_printf(out, 
		"rssfeed is a module for streaming RSS newsfeeds into a "
		"channel.\n");
	eiwic->output_printf(out, 
		"This version uses lipraptor %s (%s or %s, whatever works).\n",
		raptor_version_string, 
		RAPTOR_PARSER_NAME1, 
		RAPTOR_PARSER_NAME2);
	eiwic->output_printf(out, CMD_ADD_USAGE "\n");
	eiwic->output_printf(out, CMD_LIST_USAGE "\n");
	eiwic->output_printf(out, CMD_REMOVE_USAGE "\n");
}

int ep_unload(OUTPUT *out) {
	raptor_finish();
	return 1;
}

int ep_main(OUTPUT *out) {		
	eiwic->output_printf(out, "Initializing libraptor %s...\n",
		raptor_version_string);
	raptor_init();
	feeds = malloc(sizeof(rss_feed *));
	feeds[0] = NULL; 
	eiwic->plug_trigger_reg(CMD_ADD, TRIG_ADMIN, rss_add);
	eiwic->plug_trigger_reg(CMD_LIST, TRIG_ADMIN, rss_list);
	eiwic->plug_trigger_reg(CMD_REMOVE, TRIG_ADMIN, rss_remove);
	eiwic->output_printf(out, "rssfeed ready for casting.\n");
	return 1;
}
