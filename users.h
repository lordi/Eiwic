/******************************************************************************
 * eiwic - Extensible Ircbot Written In C
 * Copyright (C) Hannes Gräuler <lordi@styleliga.org>
 *
 * users.h: This file contains function declarations for users.c
 *****************************************************************************/
 
#ifndef _USERS_H_INCLUDED
#define _USERS_H_INCLUDED

#define MAX_NICKNAME_LEN 255

struct eiwic_user {
	u_char nick[MAX_NICKNAME_LEN];
	OUTPUT *output, *output_notice;
};

typedef struct eiwic_user USER;

typedef USER *(pfn_user_add)(STRING);
typedef void (pfn_user_nickset)(USER *, STRING);
typedef void (pfn_user_remove)(USER *);
typedef void (pfn_user_destroy)(void *);
typedef USER *(pfn_user_find)(STRING);

pfn_user_add      user_add;
pfn_user_nickset  user_nickset;
pfn_user_remove   user_remove;
pfn_user_destroy  user_destroy;
pfn_user_find     user_find;


#endif /* _USERS_H_INCLUDED */
