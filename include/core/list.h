#pragma once
#include <core/const.h>

/*Two-way linked list!*/

typedef struct ListHead{
   struct ListHead *next,*prev;
} ListHead;

#define listEntry(ptr,type,member) \
   containerOf(ptr,type,member)

inline int __listAdd(ListHead *new,ListHead *prev,ListHead *next) 
   __attribute__ ((always_inline));

inline int listAdd(ListHead *new,ListHead *prev) 
   __attribute__ ((always_inline));

inline int listAddTail(ListHead *new,ListHead *next)
   __attribute__ ((always_inline));

inline int listDelete(ListHead *del) 
   __attribute__ ((always_inline));

inline int initList(ListHead *list) 
   __attribute__ ((always_inline));

inline int listEmpty(ListHead *list) 
   __attribute__ ((always_inline));

inline int __listAdd(ListHead *new,ListHead *prev,ListHead *next)
{
   next->prev = prev->next = new;
   new->prev = prev;
   new->next = next;
   return 0;/*Successful!*/
}

inline int listAdd(ListHead *new,ListHead *prev)
{
   return __listAdd(new,prev,prev->next);
}

inline int listAddTail(ListHead *new,ListHead *next)
{
   return __listAdd(new,next->prev,next);
}

inline int listDelete(ListHead *del)
{
   ListHead *prev = del->prev;
   ListHead *next = del->next;
   next->prev = prev;
   prev->next = next;
   del->prev = del->next = del;
   return 0;
}

inline int initList(ListHead *list)
{
   list->next = list;
   list->prev = list;
   return 0;
}

inline int listEmpty(ListHead *list)
{
   return (list->next == list) && (list->prev == list);
}
