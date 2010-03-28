/*
 * tictactoe module for eiwic
 *
 * ...
 *
 *
 * by lordi@styleliga.org | http://lordi.styleliga.org/ 2004
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

EP_SETNAME("tictactoe");

#define MAXSESSIONS 25

#define USAGE "usage: !tictactoe <nickname_in_current_channel>\n"
#define ONLYONCHANNEL "Sorry, you can only start a game in a channel.\n"
#define NOMORESESSIONS "Sorry, there are too many sessions running, wait a little.\n"

typedef struct tictactoe_session {
	OUTPUT *out;
	USER *player1, *player2, *current, *winner;
	char field[3][3];
	int tics;
} SESS;

#define PLAYER1 'X'
#define PLAYER2 'O'

SESS *sessions[MAXSESSIONS];
int num_of_sessions;

int ep_help(OUTPUT *out)
{
	eiwic->output_print(out, "The tictactoe module offers high-security administrational functions\n");
	eiwic->output_print(out, "like... well no, it just lets you play TicTacToe.\n");
	eiwic->output_print(out, USAGE);

	return 1;
}

int check_for_win(SESS *s, int player)
{
	char *a = (char *)s->field, code = (player == 1? PLAYER1: PLAYER2);
	if (
	  /* horizontal */
	 (a[0] == code && a[1] == code && a[2] == code) ||
	 (a[3] == code && a[4] == code && a[5] == code) ||
	 (a[6] == code && a[7] == code && a[8] == code) ||
	  /* cross */
	 (a[0] == code && a[4] == code && a[8] == code) ||
	 (a[2] == code && a[4] == code && a[6] == code) ||
	  /* vertical */
	 (a[0] == code && a[3] == code && a[6] == code) ||
	 (a[1] == code && a[4] == code && a[7] == code) ||
	 (a[2] == code && a[5] == code && a[8] == code)
	 )
	{
		s->winner = (player == 1? s->player1: s->player2);
		return 1;
	}
  return 0;
}

int draw_field(SESS* s)
{	
	int x,y,c,i;
	
	for (x = 0; x < 3; x++)
	{
		if (x < 2) eiwic->output_print(s->out, "\037");

		for (y = 0; y < 3; y++)
		{
			c = s->field[x][y];

			if (c == 0)
				eiwic->output_printf(s->out, "%c", 'a' + x*3+y);
			else 
				eiwic->output_printf(s->out, "\002%c\002", (c == PLAYER1)?PLAYER1:PLAYER2);
				
			if (y < 2) eiwic->output_print(s->out, " | ");	
		}

		if (x < 2) eiwic->output_print(s->out, "\037\n");	
	}
	
	if (check_for_win(s, 1) || check_for_win(s, 2) || s->tics >= 9)
	{
		if (s->winner == NULL)
		  eiwic->output_printf(s->out, "\nAlright, no winner in this game.\n");
		else
		  eiwic->output_printf(s->out, "\nAnd the Winner is.... %s! Bravo!\n", s->winner->nick);
  
		for (i = 0; i < MAXSESSIONS; i++)
  		if (sessions[i] == s)
  		  sessions[i] = NULL;
		free(s);
  }
  else if (s->tics < 3)
   	eiwic->output_printf(s->out, "\n%s: Your turn, do !tic <letter>\n", s->current->nick);
	else
   	eiwic->output_printf(s->out, "\n");
   	
	return 1;
}

int on_tic_choose(char *parameter, OUTPUT *out, MSG *msg)
{
	int i, tic, letter;
	SESS *s = NULL;
	
	if (num_of_sessions == 0 || msg->user == NULL || parameter == NULL)
	{
		return 0;
	}
	
	if (strlen(parameter) > 1 || *parameter < 'a' || *parameter > 'i')
	{
		return 0;
	}
	
  for (i = 0; i < MAXSESSIONS; i++)
  	if (sessions[i])
  	{
  		if (sessions[i]->current == msg->user && sessions[i]->out == out)
  		{
  			s = sessions[i];
  			break;
  		}
  	}

	if (s == NULL) 
	{
		eiwic->output_printf(out, "%s: No, no!\n", msg->user->nick);	
		return 0;
	}

	tic = *parameter - 'a';
	letter = (s->current == s->player1 ? PLAYER1 : PLAYER2);	

	if (*((char *)s->field + tic) == PLAYER1 ||
		*((char *)s->field + tic) == PLAYER2)
	{
		eiwic->output_printf(out, "%s: No, no!\n", msg->user->nick);	
		return 0;
	}
		
	*((char *)s->field + tic) = letter;
	
	s->tics++;
	
	s->current = (s->current == s->player1 ? s->player2 : s->player1);		

	draw_field(s);
	
	return 1;
}

int on_tic_start(char *parameter, OUTPUT *out, MSG *msg)
{
	USER *enemy;
	SESS *s;
	int i;
	
	if (msg->user == NULL)
	{
		return 0;
	}

	if (msg->channel == NULL)
	{
		eiwic->output_print(out, ONLYONCHANNEL);
		return 0;
	}
	
	if (parameter == NULL || ((enemy = eiwic->user_find(parameter)) == NULL))
	{
		eiwic->output_print(out, USAGE);
		return 0;
	}

	if (num_of_sessions >= MAXSESSIONS)
	{
		eiwic->output_print(out, NOMORESESSIONS);
		return 0;
	}

	s = malloc(sizeof(SESS));

	s->out = out;
	s->tics = 0;
	
	s->player1 = enemy;
	s->player2 = msg->user;
	s->current = s->player1;
	s->winner = NULL;

	memset(s->field, 0, 3*3);

	for (i = 0; sessions[i] != NULL && i < MAXSESSIONS; i++);
	
	sessions[i] = s;
	num_of_sessions++;

	eiwic->output_printf(out, "Hey, %s, %s challenged you! Do you have the balls to play?:\n",
		s->player1->nick, s->player2->nick);
	
	draw_field(s);
}

int ep_main(OUTPUT *out)
{
	eiwic->plug_trigger_reg("!tictactoe", TRIG_PUBLIC, on_tic_start);
	eiwic->plug_trigger_reg("!tic", TRIG_PUBLIC, on_tic_choose);
	
	num_of_sessions = 0;
	memset(sessions, 0, sizeof(sessions));
	
	return 1;
}
