/*
 * adventure (eiwic module)
 *
 * ...
 *
 *
 * @author: Hannes Graeuler <lordi@styleliga.org>
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

EP_SETNAME("adventure");

#define SESSION_NONE 0
#define SESSION_INIT 1
#define SESSION_WAITINGFORPLAYERS 2
#define SESSION_PLAYING 3

#define REGISTER_MINUTES 1 // 5
#define ROUNDTIME 15 // in seconds

#define MINPLAYERS 2
#define MAXPLAYERS 10

#define MAXINVENTORYITEMS 6
#define MAXROOMITEMS 20
#define MAXROOMWAYS 10
#define MAXROOMENEMYS 10

#define ITEMNAMELENGTH 32
#define ENEMYNAMELENGTH 32
#define WAYNAMELENGTH 26
#define ROOMNAMELENGTH 26
#define ROOMMSGLENGTH 512
#define ROOMDESCLENGTH 512
#define WAYKEYMSGLENGTH 256

#define ITEM struct adventure_item
#define PLAYER struct adventure_player
#define ROOM struct adventure_room
#define ENEMY struct adventure_enemy
#define WAYOUT struct adventure_wayout
#define SESSION struct adventure_session

#define PRE "# "

#define ITEM_S "<\002\003" "12"
#define ITEM_E "\003\002>"

#define ENEMY_S "<\002\003" "4"
#define ENEMY_E "\003\002>"

#define WAY_S "<\002\003" "7"
#define WAY_E "\003\002>"

#define PLAYER_S "\002\003" "9"
#define PLAYER_E "\003\002"

#define EMPTY_S "<"
#define EMPTY_E ">"

ITEM {
	char name[ITEMNAMELENGTH];
	int p_attack, p_defend; /* 0 - 10 */
	ITEM *next;
};

ENEMY {
	char name[ENEMYNAMELENGTH];
	int p_attack, p_defend; /* 0 - 10 */
	int livepoints;
	ITEM *dropitem;
	ENEMY *next;
};

PLAYER {
	int active;
	USER *user;
	ITEM *weapon, *inventory[MAXINVENTORYITEMS];
	int livepoints; /* 100 - 0 (dead) */
	int attacked;
	int died;
	int total_damage_made, total_kills;
};

WAYOUT {
	int active;
	char name[WAYNAMELENGTH];
	ITEM *key;
	int key_count; /* 0 = can't enter with key,
	                  1 - ... = need key_count items */
	char key_msg[WAYKEYMSGLENGTH];
	ROOM *room;
};

ROOM {
	char name[ROOMNAMELENGTH];
	char msg[ROOMMSGLENGTH];
	char msga[ROOMMSGLENGTH];
	char desc[ROOMDESCLENGTH];
	WAYOUT ways[MAXROOMWAYS];
	ITEM *items[MAXROOMITEMS];
	ENEMY *enemys[MAXROOMENEMYS];
	int enemys_lp[MAXROOMENEMYS];
	int visited, defined;
	ROOM *next;
};

SESSION {
	OUTPUT *out;
	PLAYER players[MAXPLAYERS];
	ROOM *rooms, 
	  *next_room,
		*current_room,
		*previous_room,
		*start_room,
		*end_room;
	ITEM *items;
	ENEMY *enemys;
	int status;
	int player_max, player_min, player_count;
	int looked_at_room;
	TIMER *t_round;
};

SESSION session;

int adventure_init()
{
	memset(&session, 0, sizeof(SESSION));
	session.status = SESSION_NONE;
}

int adventure_deinit()
{
	/* TODO: deallocate memory */
	adventure_init();
}

ENEMY *adventure_find_enemy(char *name)
{
	ENEMY *i;
	for (i = session.enemys; i; i = i->next)
		if (strcmp(i->name, name) == 0) break;
	return i;	
}

ITEM *adventure_find_item(char *name)
{
	ITEM *i;
	for (i = session.items; i; i = i->next)
		if (strcmp(i->name, name) == 0) break;
	return i;	
}

ROOM *adventure_find_room(char *name)
{
	ROOM *rl;

	for (rl = session.rooms; rl; rl = rl->next)
	{
		if (strcmp(rl->name, name) == 0) break;
	}
	
	return rl;
}

WAYOUT *adventure_find_way(ROOM *current, char *name)
{
	int i;
	
	for (i = 0; i < MAXROOMWAYS; i++)
	{
		if (current->ways[i].active == 1 && 
			strcmp(current->ways[i].name, name) == 0)
		{
			return &current->ways[i];
		}
	}
	
	return NULL;
}

ROOM *adventure_create_room(char *name)
{
  ROOM *r, *rl;

	r = malloc(sizeof(ROOM));
	memset(r, 0, sizeof(ROOM));
	strncpy(r->name, name, ROOMNAMELENGTH);
	r->name[ROOMNAMELENGTH] = '\0';
	r->next = NULL;
	r->visited = 0;
	r->defined = 0;
			
	if (session.rooms == NULL)
	{
		session.rooms = r;
	}
	else
	{
		rl = session.rooms;
		while (rl->next != NULL) rl = rl->next;
		rl->next = r;
	}

	return r;
}

PLAYER* adventure_random_player()
{
	int p,num=0,chosenone;
	
	for (p = 0; p < MAXPLAYERS; p++)
	{
		if (session.players[p].active > 0)
		{
			num++;
		}
	}
	
	chosenone = rand() % num;

	for (p = 0, num = 0; p < MAXPLAYERS; p++)
	{
		if (session.players[p].active > 0)
		{
			if (chosenone == num++)
				return &(session.players[p]);
		}
	}
	
	return NULL;
}


void adventure_end()
{
  eiwic->output_printf(session.out, PRE "You made it through! Congratulations 8->-\n");
  eiwic->output_printf(session.out, PRE "/* TODO: Player rankings */\n");
  eiwic->output_printf(session.out, PRE "Adventure ended.\n");
  
  adventure_deinit();
}

void adventure_damage_player(PLAYER *p_victim, int damage)
{
  p_victim->livepoints -= damage;
  
  if (p_victim->livepoints < 1)
  {
  	p_victim->active = 0;
  	p_victim->died = 1;
    eiwic->output_printf(session.out, PRE "Player %s died. :/\n", p_victim->user->nick);
  }
}

void adventure_show_room(int look);

void adventure_end_round(void *useless)
{
	int p,att=0,i,real,damage;
	PLAYER *pl;
	ENEMY *e;
	ROOM *room = session.current_room;

  session.looked_at_room = 0;
  
  /* enemies attack! */
 	for (i = 0; i < MAXROOMENEMYS; i++)
	{
		if (room->enemys_lp[i] > 0 && room->enemys[i])
		{
			e = room->enemys[i];
			pl = adventure_random_player();
			
			if (rand() % 15 == 0)
				real = -1;
			else
			{
	  		damage = rand() % e->p_attack + 1;
	  		if (damage > e->p_attack) damage = e->p_attack;
  			real = (pl->weapon?damage - pl->weapon->p_defend:damage);
	  		if (real < 0) real = 0;
			}

			eiwic->output_printf(session.out, 
				PRE
				ENEMY_S "%s" ENEMY_E
				" attacks "
				PLAYER_S "%s" PLAYER_E
				": ",
				room->enemys[i]->name, pl->user->nick);
			
  		if (real > 0)
    		eiwic->output_printf(session.out, "-%d LP!\n", real);
  		else if (real == -1)
      	eiwic->output_printf(session.out, "Missed!\n");
    	else
      	eiwic->output_printf(session.out, "No damage!\n");

			adventure_damage_player(pl, real);
		}
	}

  
	for (p = 0; p < MAXPLAYERS; p++)
	{
		if (session.players[p].active > 0)
		{
			if (session.players[p].attacked) att++;
			session.players[p].attacked = 0;
		}
	}

 	if (att > 0)
 		eiwic->output_printf(session.out, PRE "-\n");
  	
	session.t_round
		  = eiwic->plug_timer_reg(time(NULL) + ROUNDTIME, adventure_end_round, NULL);

  if (session.next_room != NULL)
  {	
 		session.current_room->visited = 1;
	  session.previous_room = session.current_room;	
	  session.current_room = session.next_room;
	  session.next_room = NULL;

  	adventure_show_room(0);	
	}
}

void adventure_show_room(int look)
{
	int i, f = 0;
	ROOM *room = session.current_room;
	
	if (look)
	{
    session.looked_at_room = 1;
  	eiwic->output_printf(session.out, PRE "%s\n", room->desc);
  	eiwic->output_printf(session.out, PRE "Items: ");
  	
  	for (i = 0; i < MAXROOMITEMS; i++)
  	{
  		if (room->items[i])
  		{
			  if (f++!=0) eiwic->output_print(session.out, ", ");	
		    eiwic->output_printf(session.out, ITEM_S "%s" ITEM_E, room->items[i]->name);	
		  }
  	}

		if (f == 0) eiwic->output_print(session.out, EMPTY_S "none" EMPTY_E "\n");
		else
		{
    	eiwic->output_print(session.out, "\n");	
 	  	f = 0;
 	  }
  }
	else
	{
		if (session.t_round)
		{
			eiwic->plug_timer_unreg(session.t_round);
	  }
	
	if (room->visited == 1)
	{
  	eiwic->output_printf(session.out, PRE "%s\n", room->msga);	
  }
  else
	  eiwic->output_printf(session.out, PRE "%s\n", room->msg);	
	}
	
	
		if (room == session.end_room)
		{
			adventure_end();
			return;
			
		}
	 
		session.t_round
		  = eiwic->plug_timer_reg(time(NULL) + ROUNDTIME, adventure_end_round, NULL);
	 
	/* print out all directions in this froom */ 
	eiwic->output_print(session.out, PRE "[");	
	for (i = 0; i < MAXROOMWAYS; i++)
	{
		if (room->ways[i].active)
		{
			if (f++!=0) eiwic->output_print(session.out, ", ");	
		  eiwic->output_printf(session.out, WAY_S "%s" WAY_E, room->ways[i].name);	
		}
	}
	eiwic->output_print(session.out, "]\n");	
	
		
	/* if there're enemies, print em out, too */
	for (i = 0; i < MAXROOMENEMYS; i++)
	{
		if (room->enemys_lp[i] > 0 && room->enemys[i])
		{
			eiwic->output_printf(session.out, PRE "Enemy: " ENEMY_S "%s" ENEMY_E " (%d LP)\n",
				room->enemys[i]->name, room->enemys_lp[i]);
		}
	}
}

int adventure_setup_way(WAYOUT *way, char *p1, char *p2, char *p3, char *p4, char *p5)
{
	if (p1 == NULL || p2 == NULL) return 0;
	
	strncpy(way->name, p1, WAYNAMELENGTH);
	way->name[WAYNAMELENGTH] = '\0';
	
	way->room = adventure_find_room(p2);	
	if (way->room == NULL) way->room = adventure_create_room(p2);
	
	if (p3 != NULL)
	{
		way->key = adventure_find_item(p3);
		way->key_count = p4 != NULL ? atoi(p4) : 1;
		
		if (p5 != NULL)
		{
			strncpy(way->key_msg, p5, WAYKEYMSGLENGTH);
			way->key_msg[WAYKEYMSGLENGTH] = '\0';
		}
	}	
	way->active = 1;
	
	return 1;
}

int adventure_load(char *adf)
{
	FILE *f;
	ITEM *item, *il;
	ENEMY *enemy, *el;
	ROOM *room;
	char line[1024], *p, *eol, *tok;
	char *param1, *param2, *param3, *param4, *param5;
	int err = 0, i;
	int ic = 0, ec = 0, rc = 0, lc = 0;
	
	if ((f = fopen(adf, "r")) == NULL)
	{
		eiwic->output_printf(session.out, PRE "Unable to open file '%s'!\n", adf);	
		return 0;
	}

	while ((p = fgets(line, sizeof(line) - 1, f)) != NULL)
	{
		lc++;

		eol = p;
		
		/* remove \r\n or \n at end of line */
		while (*eol != '\r' && *eol != '\n') eol++;
		*eol = '\0';
		
		while (*p == ' ') p++; /* clear out spaces from beginning of line */
		
		/* leave out comments and empty lines */
		if (*p == '#' || strlen(p) == 0) continue; 

		if ((tok = strtok(p, ":")) == NULL)  { err = 1; break; }

		param1 = strtok(NULL, ";");
		if (param1 != NULL) param2 = strtok(NULL, ";");
		if (param2 != NULL) param3 = strtok(NULL, ";");
		if (param3 != NULL) param4 = strtok(NULL, ";");
		if (param4 != NULL) param5 = strtok(NULL, ";");

		if (strcmp(tok, "RW") == 0 && room && param1)
		{
			for (i = 0;  i < MAXROOMWAYS && room->ways[i].active; i++);
			adventure_setup_way(&room->ways[i], param1, param2, param3, param4, param5);
			if (room->ways[i].active == 0) { err = 1; break; }
		}
		else
		if (strcmp(tok, "PMAX") == 0 && param1)
			session.player_max = atoi(param1);
		else
		if (strcmp(tok, "PMIN") == 0 && param1)
			session.player_min = atoi(param1);
		else
		if (strcmp(tok, "REND") == 0 && room)
			session.end_room = room;
		else
		if (strcmp(tok, "RSTART") == 0 && room)
			session.start_room = room;
		else
		if (strcmp(tok, "RE") == 0 && room && param1)
		{
			for (i = 0;  i < MAXROOMENEMYS && room->enemys[i]; i++);
			room->enemys[i] = adventure_find_enemy(param1);
			if (room->enemys[i] == NULL) { err = 1; break; }
			room->enemys_lp[i] = room->enemys[i]->livepoints;
		}
		else
		if (strcmp(tok, "RI") == 0 && room && param1)
		{
			for (i = 0;  i < MAXROOMITEMS && room->items[i]; i++);
			room->items[i] = adventure_find_item(param1);
			if (room->items[i] == NULL) { err = 1; break; }
		}
		else
		if (strcmp(tok, "RDESC") == 0 && room && param1)
		{
			strncpy(room->desc, param1, ROOMDESCLENGTH);
			room->desc[ROOMDESCLENGTH] = 0;
		}
		else
		if (strcmp(tok, "RMSGA") == 0 && room && param1)
		{
			strncpy(room->msga, param1, ROOMMSGLENGTH);
			room->msga[ROOMMSGLENGTH] = 0;
		}
		else
		if (strcmp(tok, "RMSG") == 0 && room && param1)
		{
			strncpy(room->msg, param1, ROOMMSGLENGTH);
			room->msg[ROOMMSGLENGTH] = 0;
		}
		else
		if (strcmp(tok, "R") == 0 && param1)
		{
			room = adventure_find_room(param1);
			if (room == NULL) room = adventure_create_room(param1);
			room->defined = 1;
			rc++;
		}
		else
		if (strcmp(tok, "I") == 0 && param1 && param2 && param3)
		{
			item = malloc(sizeof(ITEM));
			strncpy(item->name, param1, ITEMNAMELENGTH);
			item->name[ITEMNAMELENGTH] = '\0';
			item->p_attack = atoi(param2);
			item->p_defend = atoi(param3);
			item->next = NULL;
			
			if (session.items == NULL)
			{
				session.items = item;
			}
			else
			{
				il = session.items;
				while (il->next != NULL) il = il->next;
				il->next = item;
			}

			ic++;
		}
		else
		if (strcmp(tok, "E") == 0 && param1 && param2 && param3 && param4)
		{
			enemy = malloc(sizeof(ENEMY));
			strncpy(enemy->name, param1, ENEMYNAMELENGTH);
			enemy->name[ENEMYNAMELENGTH] = '\0';
			enemy->p_attack = atoi(param2);
			enemy->p_defend = atoi(param3);
			enemy->livepoints = atoi(param4);
			enemy->next = NULL;
			enemy->dropitem =
				((param5 != NULL) ? adventure_find_item(param5) : NULL);
			
			if (session.enemys == NULL)
			{
				session.enemys = enemy;
			}
			else
			{
				el = session.enemys;
				while (el->next != NULL) el = el->next;
				el->next = enemy;
			}

			ec++;
		}
		else
		{
			err = 1;
			break;
		}
	}	

	if (err)
	{
		eiwic->output_printf(session.out, PRE "Parse error in line %d!\n", lc);	
		return 0;
	}
	else
	{
		eiwic->output_printf(session.out, PRE
			"Loaded adventure (%d items, %d enemys, %d rooms), checking...\n",
			ic, ec, rc);	
			
		if (session.start_room == NULL)
		{
			eiwic->output_printf(session.out, PRE "No start room defined!\n");	
			return 0;
		}
		
		if (session.end_room == NULL)
		{
			eiwic->output_printf(session.out, PRE "No end room defined!\n");	
			return 0;
		}
		
		for (room = session.rooms; room; room = room->next)
			if (room->defined == 0)
  		{
  			eiwic->output_printf(session.out, PRE "Room '%s' is not defined!\n",
  				room->name);	
	  		return 0;
	  	}
		
		eiwic->output_printf(session.out, PRE "OK!\n");	
		
		return 1;	
	}
}

int adventure_exit()
{
	session.status = SESSION_NONE;
	/* TODO: clean up. */
}

ITEM *adventure_get(ITEM *item)
{
	int i;
	ROOM *r = session.current_room;
	
	for (i = 0; i < MAXROOMITEMS; i++)
	{
		if (r->items[i] == item)
		{
			r->items[i] = NULL;
			return item;
		}
	}
	
	return NULL;
}

int adventure_register_user(USER *user)
{
	PLAYER *player;
	int i;
	
	if (session.player_count >= session.player_max)
	{
	  eiwic->output_printf(session.out,
	  	"%s: Sorry, this adventure only allows up to %d players.\n",
	  	user->nick, session.player_max);	
		return 0;
	}
	
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (session.players[i].active == 1 && session.players[i].user == user)
		{
			return 0;
		}
	}

	/* search for a free slot */
	for (i = 0; i < MAXPLAYERS
		&& session.players[i].active == 1; i++);
	
	player = &(session.players[i]);
	memset(player, 0, sizeof(PLAYER));
	player->active = 1;	
	player->livepoints = 100;
	player->user = user;
	
  eiwic->output_printf(session.out,
  	PRE "Player added: " PLAYER_S "%s" PLAYER_E " [%d/%d].\n",
  	user->nick, ++session.player_count, session.player_max);	

	return 1;
}

void on_end_register(void *opt)
{
	int i;
	
	eiwic->output_printf(session.out, PRE
		"Time's up! %d players registred for this game.\n",
		session.player_count);	

	if (session.player_count < session.player_min)
	{
		eiwic->output_printf(session.out, PRE
			"Sorry, there must be at least %d player(s), aborting...\n",
			session.player_min);	
		adventure_exit();
		return;
	}
	
	eiwic->output_print(session.out, PRE);
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (session.players[i].active == 1)
		{
		  if (i > 0) eiwic->output_print(session.out, ", ");
		  eiwic->output_printf(session.out, "%s", session.players[i].user->nick);
		}
	}
	
	eiwic->output_printf(session.out, ": Get ready....!\n");	

	session.current_room = session.start_room;
	session.status = SESSION_PLAYING;
	adventure_show_room(0);
}

int adventure_start(char *adf, OUTPUT *out)
{
	adventure_init();
	session.out = out;
	session.status = SESSION_INIT;
	
	if (!adventure_load(adf))
	{
		adventure_init();
		return 0;
	}
	
  eiwic->output_printf(session.out, PRE
  	"Adventure loaded. You now have %d minute(s) to register yourself as a\n", REGISTER_MINUTES);	
  eiwic->output_printf(session.out, PRE
  	"player by typing !play. The adventure will start in %d minute(s).\n", REGISTER_MINUTES);
  
	session.status = SESSION_WAITINGFORPLAYERS;

  eiwic->plug_timer_reg(time(NULL) + REGISTER_MINUTES * 60, on_end_register, NULL);
}

int adventure_status(OUTPUT *out, int status)
{
	if (session.out != out) return 0;
	if (session.status != status) return 0;
	
	return 1;
}

PLAYER *adventure_player(USER *user)
{
	int p = 0;
	
	if (user == NULL) return NULL;
	
	for (; p < MAXPLAYERS; p++)
	{
		if (session.players[p].livepoints > 0)
		{
			if (session.players[p].user == user)
				return &(session.players[p]);
		}
	}
	
	eiwic->output_printf(session.out,
		"%s: You aren't a player in this session.\n",
		user->nick);
	
	return NULL;
}


/* eiwic module handlers: */

int on_play(char *parameter, OUTPUT *out, MSG *msg)
{
	if (!adventure_status(out, SESSION_WAITINGFORPLAYERS)) return 0;
	
	return (msg->user?adventure_register_user(msg->user):0);
}

int on_get(char *parameter, OUTPUT *out, MSG *msg)
{
	PLAYER *player;
	ITEM *item;
	int i, frei = -1;

	if (!adventure_status(out, SESSION_PLAYING)) return 0;
	if ((player = adventure_player(msg->user)) == NULL) return 0;
	
	for (i = 0; i < MAXINVENTORYITEMS; i++)
	{
		if (player->inventory[i] == NULL) { frei = i; break; }
	}
	
	if (frei == -1)
	{
		eiwic->output_printf(session.out, PRE "%s, you can't carry anymore!\n",
    	player->user->nick);
	}
	
	if ((item = adventure_find_item(parameter)) == NULL)
	{
		eiwic->output_printf(session.out, PRE "%s, can't find what you've been looking for!\n",
    	player->user->nick);
		return 0;
	}
	
	if ((item = adventure_get(item)) != NULL)
	{
		player->inventory[frei] = item;
		eiwic->output_printf(session.out, PRE
			PLAYER_S "%s" PLAYER_E " gets " ITEM_S "%s" ITEM_E ".\n",
    	player->user->nick, item->name);
  	return 1;
  }
  else
 	{
		eiwic->output_printf(session.out, PRE "%s, can't find what you've been looking for!\n",
    	player->user->nick);
		return 0;
 	}

	return 1;
}

int on_put(char *parameter, OUTPUT *out, MSG *msg)
{
	PLAYER *player;
	ITEM *item, *found = NULL;
	ROOM *room = session.current_room;
	int i, frei = -1;

	if (!adventure_status(out, SESSION_PLAYING)) return 0;
	if ((player = adventure_player(msg->user)) == NULL) return 0;
	if (parameter == NULL) return 0;
	
	if ((item = adventure_find_item(parameter)) == NULL)
	{
		eiwic->output_printf(session.out, PRE "%s, you have no such thing!\n",
    	player->user->nick);
		return 0;
	}
	
	for (i = 0; i < MAXROOMITEMS; i++)
	{
		if (room->items[i] == NULL)
		{
			frei = i;
			break;
		}
	}
	
	if (frei == -1)
	{
		eiwic->output_printf(session.out, PRE "%s, room is full already.!\n",
    	player->user->nick);
		return 0;
	}
	
	for (i = 0; i < MAXINVENTORYITEMS; i++)
	{
		if (player->inventory[i] == item)
		{
			player->inventory[i] = NULL;
			found = item;
			break;
		}
	}
	
	if (found == NULL)
	{
		if (player->weapon == item)
		{
			player->weapon = NULL;
			found = item;
		}
		else
		{
			eiwic->output_printf(session.out, PRE "%s, you have no such thing!\n",
  	  	player->user->nick);
			return 0;
		}
	}
	
	room->items[frei] = found;

	eiwic->output_printf(session.out, PRE
		PLAYER_S "%s" PLAYER_E " puts " ITEM_S "%s" ITEM_E " to the floor.\n",
 	  player->user->nick, found->name);
	return 1;
}

int on_weapon(char *parameter, OUTPUT *out, MSG *msg)
{
	PLAYER *player;
	ITEM *item;

	if (!adventure_status(out, SESSION_PLAYING)) return 0;
	if ((player = adventure_player(msg->user)) == NULL) return 0;
	if (parameter == NULL) return 0;
	
	if (player->weapon != NULL)
	{
		eiwic->output_printf(session.out, "%s, you already have a weapon!\n",
    	player->user->nick);
		return 0;
	}

	if ((item = adventure_find_item(parameter)) == NULL)
	{
		eiwic->output_printf(session.out, "%s, can't find what you've been looking for!\n",
    	player->user->nick);
		return 0;
	}
	
	if ((player->weapon = adventure_get(item)) != NULL)
	{
		eiwic->output_printf(session.out, PRE PLAYER_S "%s" PLAYER_E " now fighting with "
			ITEM_S "%s" ITEM_E " (Att=%d, Def=%d).\n",
    	player->user->nick, player->weapon->name,
    	player->weapon->p_attack, player->weapon->p_defend);
  	return 1;
  }
  else
 	{
		eiwic->output_printf(session.out, "%s, can't find what you've been looking for!\n",
    	player->user->nick);
		return 0;
 	}
}

int on_attack(char *parameter, OUTPUT *out, MSG *msg)
{
	PLAYER *player, *p_victim = NULL;
	ENEMY *e_victim = NULL;
	int victim_id = 0;
	ROOM *room = session.current_room;
	int real,damage,i;
	char *m;

	if (!adventure_status(out, SESSION_PLAYING)) return 0;
	if ((player = adventure_player(msg->user)) == NULL) return 0;
	if (player->attacked == 1) return 0;
	if (player->weapon == NULL) return 0;
	
	m = strtok(parameter, " ");
	
	if (m == NULL) return 0;

	for(i = 0; i < MAXPLAYERS; i++)
	{
		if (session.players[i].active)
		{
			if (strcmp(m, session.players[i].user->nick) == 0)
			{
				p_victim = &(session.players[i]);
				break;
			}
		}
	}
	
	if (p_victim == NULL)
	{
  	for(i = 0; i < MAXROOMENEMYS; i++)
	  {
	  	if (room->enemys_lp[i] > 0 && room->enemys[i])
			{
				if (strcmp(m, room->enemys[i]->name) == 0)
				{
					e_victim = room->enemys[i];
					victim_id = i;
					break;
				}
			}  	
	  }

  	if (e_victim == NULL)
  	{
		  eiwic->output_printf(session.out, "%s, huh? Can't find '%s'!\n",
  			player->user->nick, m);
		  return 0;
	  }
  }
	
	if (rand() % 15 == 0)
		real = -1;
	else
	{
	  damage = ((rand() % 2) + 1) * (rand() % player->weapon->p_attack);
	  if (damage > player->weapon->p_attack) damage = player->weapon->p_attack;
	  
  	if (p_victim)
	  real = (p_victim->weapon?damage - p_victim->weapon->p_defend:damage);
	  else
	  real = damage - e_victim->p_defend;
	  
	  if (real < 0) real = 0;
	}

	if (p_victim)
	{	
		eiwic->output_printf(session.out, PRE 
			PLAYER_S "%s" PLAYER_E " attacks "
			PLAYER_S "%s" PLAYER_E " with "
			ITEM_S "%s" ITEM_E ": ",
    	player->user->nick, p_victim->user->nick,
    	player->weapon->name);
  }
  else
  {
		eiwic->output_printf(session.out, PRE 
			PLAYER_S "%s" PLAYER_E " attacks "
			ENEMY_S "%s" ENEMY_E " with "
			ITEM_S "%s" ITEM_E ": ",
  	  player->user->nick, e_victim->name,
    	player->weapon->name);
  }
    
  if (real > 0)
    eiwic->output_printf(session.out, "-%d LP!\n", real);
  else if (real == -1)
    eiwic->output_printf(session.out, "Missed!\n", real);
  else
    eiwic->output_printf(session.out, "No damage!\n", real);
    
  player->total_damage_made += real;
  
  if (p_victim)
  {
  	if (real > 0) adventure_damage_player(p_victim, real);
  }
  else
  {
  	if (real > 0) room->enemys_lp[victim_id] -= real;
  	if (room->enemys_lp[victim_id] < 1)
  	{  	
      eiwic->output_printf(session.out, PRE ENEMY_S "%s" ENEMY_E " was killed by "
      	PLAYER_S "%s" PLAYER_E "!\n",
    	  room->enemys[victim_id]->name, player->user->nick);
  		room->enemys[victim_id] = NULL;
    	player->total_kills++;
    }
  }

	player->attacked = 1;

	return 1;
}

int on_look(char *parameter, OUTPUT *out, MSG *msg)
{
	PLAYER *player;

	if (!adventure_status(out, SESSION_PLAYING)) return 0;
	if ((player = adventure_player(msg->user)) == NULL) return 0;
	
	if (session.looked_at_room == 0)	adventure_show_room(1);	

	return 1;
}

int on_goto(char *parameter, OUTPUT *out, MSG *msg)
{
	PLAYER *player;
	ROOM *room;
	WAYOUT *way;
	int enemy_count = 0;

	if (!adventure_status(out, SESSION_PLAYING)) return 0;
	if ((player = adventure_player(msg->user)) == NULL) return 0;
	if (parameter == NULL) return 0;
	
	way = adventure_find_way(room = session.current_room, parameter);
	
	if (way == NULL) return 0;
	
	/* check if enemys are blocking the ways, however the
	 * players can go the way they came from no matter if there
	 * are still enemys living 
	 */
	if (way->room != session.previous_room)
	{
		int i;
   	for (i = 0; i < MAXROOMENEMYS; i++)
	  	if (room->enemys_lp[i] > 0 && room->enemys[i])
			  enemy_count++;

	  if (enemy_count > 0)
	  {
      eiwic->output_printf(session.out, PRE
      	WAY_S "%s" WAY_E ": There are enemys in the way.\n",
      	way->name);
      return 0;
	  }
	}
	
	if (way->key != NULL)
	{
    eiwic->output_printf(session.out, PRE "%s\n",
    	way->key_msg);
		return 0;
	}
	
	session.next_room = way->room;

  eiwic->output_printf(session.out, PRE "Moving to " WAY_S "%s" WAY_E " at end of round...\n",
  	way->name);
	return 1;
}

int on_me(char *parameter, OUTPUT *out, MSG *msg)
{
	PLAYER *player;
	int i;
	
	if (!adventure_status(out, SESSION_PLAYING)) return 0;
	if ((player = adventure_player(msg->user)) == NULL) return 0;
	
  eiwic->output_printf(session.out,
		PRE "( " PLAYER_S "%s" PLAYER_E " lp: %d/100, weapon: ",
		player->user->nick, player->livepoints);
	
	if (player->weapon)
	{
    eiwic->output_printf(session.out,
	  	ITEM_S "%s" ITEM_E " (%d/%d)", player->weapon->name,
	  	player->weapon->p_attack, player->weapon->p_defend);
	}
  else
  {
    eiwic->output_printf(session.out, EMPTY_S "none" EMPTY_E);
  }
  
  eiwic->output_printf(session.out, ", inventory: ");
  
  for (i = 0; i < MAXINVENTORYITEMS; i++)
  {
  	if (i > 0) eiwic->output_printf(session.out, ", ");
  	if (player->inventory[i])
  		eiwic->output_printf(session.out, ITEM_S "%s" ITEM_E,
  			player->inventory[i]->name);
  	else
  		eiwic->output_printf(session.out, EMPTY_S "none" EMPTY_E);
  }
  
  eiwic->output_printf(session.out, " )\n");
  
	return 1;
}

int on_start(char *parameter, OUTPUT *out, MSG *msg)
{
	if (parameter == NULL)
	{
		eiwic->output_printf(out, "usage: !adventure <filename>\n");
		return 0;
	}
	
	if (session.status != SESSION_NONE)
	{
		eiwic->output_printf(out, "There already is a adventure session running.\n");
		return 0;
	}
	
	adventure_start(parameter, out);
	
	return 1;
}

int ep_main(OUTPUT *out)
{
	adventure_init();
	
	eiwic->plug_trigger_reg("!adventure", TRIG_PUBLIC, on_start);
	/* init adventure */
	
	eiwic->plug_trigger_reg("!play", TRIG_PUBLIC, on_play);
	/* register as a player during init time */

	eiwic->plug_trigger_reg("!go", TRIG_PUBLIC, on_goto);
	eiwic->plug_trigger_reg("!goto", TRIG_PUBLIC, on_goto);
	/* goto another room */
	
	eiwic->plug_trigger_reg("!g", TRIG_PUBLIC, on_get);
	eiwic->plug_trigger_reg("!get", TRIG_PUBLIC, on_get);
	/* get an item from the floor to your inventory */
	
	eiwic->plug_trigger_reg("!w", TRIG_PUBLIC, on_weapon);
	eiwic->plug_trigger_reg("!weapon", TRIG_PUBLIC, on_weapon);
	/* grab an item from the floor and use it as a weapon */

	eiwic->plug_trigger_reg("!p", TRIG_PUBLIC, on_put);
	eiwic->plug_trigger_reg("!put", TRIG_PUBLIC, on_put);
	/* put an item from your inventory or your weapon on the floor */
	
	eiwic->plug_trigger_reg("!a", TRIG_PUBLIC, on_attack);
	eiwic->plug_trigger_reg("!attack", TRIG_PUBLIC, on_attack);
	/* attack an enemy */

	eiwic->plug_trigger_reg("!l", TRIG_PUBLIC, on_look);
	eiwic->plug_trigger_reg("!look", TRIG_PUBLIC, on_look);
	/* look around in the current room */
	
	eiwic->plug_trigger_reg("!m", TRIG_PUBLIC, on_me);
	eiwic->plug_trigger_reg("!me", TRIG_PUBLIC, on_me);
	/* shows your current life points, your weapon and your inventory */

	return 1;
}

int ep_help(OUTPUT *out)
{
	eiwic->output_print(out, "The adventure module # Help text\n");
	eiwic->output_print(out, "-\n");
	eiwic->output_print(out, "To play an adventure in your IRC channel, gather\n");
	eiwic->output_print(out, "some people, type !adventure <file.txt> and simply\n");
	eiwic->output_print(out, "follow the instructions.\n");
	eiwic->output_print(out, "-\n");
	eiwic->output_print(out, "During the game, following triggers can be used:\n");
	eiwic->output_print(out, " * !goto/!go <way> -- Your group will move the selected way.\n");
	eiwic->output_print(out, " * !get/!g <itemname> -- Gets an item from the floor and puts\n");
	eiwic->output_print(out, "      it in your inventory.\n");
	eiwic->output_print(out, " * !weapon/!w <itemname> -- Gets an item from the floor and\n");
	eiwic->output_print(out, "      puts it in your hand as a weapon.\n");
	eiwic->output_print(out, " * !put/!p <itemname> -- Puts an item from your inventory or\n");
	eiwic->output_print(out, "      your weapon to the floor.\n");
	eiwic->output_print(out, " * !attack/!a <enemy/nick> -- Attacks an enemy or player\n");
	eiwic->output_print(out, " * !look/!l -- Detailed room information (items, etc).\n");
	eiwic->output_print(out, " * !me/!m -- Display user information (inventory, health, etc).\n");
	eiwic->output_print(out, "-\n");

	return 1;
}

