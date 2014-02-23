#pragma once
#include <core/const.h>

typedef struct HashListHead HashListHead;
typedef struct HashListNode HashListNode;

typedef struct HashListHead{
   HashListNode *first;
} HashListHead;

typedef struct HashListNode{
   HashListNode **pprev,*next;
} HashListNode;

#define hashListEntry(a,b,c) \
   containerOf(a,b,c)

inline int initHashListHead(HashListHead *head)
                __attribute__ ((always_inline));
inline int initHashListNode(HashListNode *node)
                __attribute__ ((always_inline));
inline int hashListHeadAdd(HashListNode *node,HashListHead *head)
                __attribute__ ((always_inline));
inline int hashListDelete(HashListNode *node)
                __attribute__ ((always_inline));
inline int hashListEmpty(HashListNode *node)
                __attribute__ ((always_inline));

inline int initHashListNode(HashListNode *node)
{
   node->pprev = &node->next;
   *node->pprev = 0;
   return 0;
}

inline int initHashListHead(HashListHead *head)
{
   head->first = 0;
   return 0;
}

inline int hashListEmpty(HashListNode *node)
{ /*This function checks if this node is used.*/
  /*Before using this function,we must call the 'initHashListNode' function first.*/
   return node->pprev == &node->next;
}

inline int hashListHeadAdd(HashListNode *node,HashListHead *head)
{
   node->next = head->first;
   node->pprev = &head->first;
   head->first = node;
   return 0;
}

inline int hashListDelete(HashListNode *node)
{
   if(node->next) /*Last?*/
      node->next->pprev = node->pprev;
   *node->pprev = node->next;
   return 0;
}
