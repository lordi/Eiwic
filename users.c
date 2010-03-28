/******************************************************************************
 * eiwic - Extensible Ircbot Written In C
 * Copyright (C) Hannes Gräuler <lordi@styleliga.org>
 *
 * users.c: This file contains functions to manage users on IRC
 *****************************************************************************/
 
#include "eiwic.h"
#include "plugins.h"
#include "users.h"


USER *user_add(STRING name)
{
	USER *user;
	
	if ((user = user_find(name)) != NULL)
		return user;

	user = malloc(sizeof(USER));
	bzero(user, sizeof(USER));

	user_nickset(user, name);

	dlist_ins_next(&e_global->users, dlist_tail(&e_global->users), user);
	
#ifdef VV_DEBUG
	log(LOG_VERBOSE_DEBUG, "User '%s' has been added.", user->nick);
#endif

	return user;
}

void user_nickset(USER *user, STRING newnick)
{
	strncpy(user->nick, newnick, sizeof(user->nick)- 1);
}

void user_remove(USER *user)
{
	if (user)
		dlist_remove_and_destroy(&e_global->users, user);

	return;
}

void user_destroy(void *data)
{
	USER *user = data;

	free(user);
}

USER *user_find(STRING name)
{
	DListElmt *el;

	FOR_EACH(&e_global->users, el)
		if (strcasecmp(((USER *)el->data)->nick, name) == 0)
			return (USER *)el->data;
		
	return NULL;
}

/*--- end of channel functions ---*/

