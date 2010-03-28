/*****************************************************************************
 * Notice:                                                                   *
 * This source has been taken from : Algorithmen mit C von Kyle Loudon,      *
 * O'Reilly & Associates (ISBN: 3-89721-165-3).                              *
 ****************************************************************************/

#ifndef DLIST_H
#define DLIST_H

#include <stdlib.h>

typedef struct DListElmt_ {
	void               *data;
	struct DListElmt_  *prev;
	struct DListElmt_  *next;
} DListElmt;

typedef struct DList_ {
	int                size;
	int                (*match)(const void *key1, const void *key2);
	void               (*destroy)(void *data);
	DListElmt          *head;
	DListElmt          *tail;
} DList;

DListElmt *dlist_find(DList *list, const void *data); /* lordi@styleliga.org */
void dlist_init(DList *list, void (*destroy)(void *data));
void dlist_destroy(DList *list);
int dlist_ins_next(DList *list, DListElmt *element, const void *data);
int dlist_ins_prev(DList *list, DListElmt *element, const void *data);
int dlist_remove(DList *list, DListElmt *element, void **data);
#define dlist_remove_and_destroy(list, data)\
{ void *data2;\
	dlist_remove((list), dlist_find((list), (data)), &data2);\
	(*(list)).destroy(data2);\
 }	
	
#define dlist_size(list) ((list)->size)
#define dlist_head(list) ((list)->head)
#define dlist_tail(list) ((list)->tail)
#define dlist_is_head(element) ((element)->prev == NULL ? 1 : 0)
#define dlist_is_tail(element) ((element)->next == NULL ? 1 : 0)
#define dlist_data(element) ((element)->data)
#define dlist_next(element) ((element)->next)
#define dlist_prev(element) ((element)->prev)

#define FOR_EACH(list, element) for((element) = (list)->head; (element) && (element)->data; (element) = (element)->next)
#define FOR_EACH_DATA(list, obj) { DListElmt* element; FOR_EACH(list, element) { (obj)=(element)->data;
#define END_DATA } }

#endif
