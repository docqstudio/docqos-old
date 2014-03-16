#pragma once
#include <core/const.h>
#include <cpu/spinlock.h>

typedef struct RadixTreeNode RadixTreeNode;

typedef struct RadixTreeNode
{
   unsigned int count;
   unsigned int nr;
   RadixTreeNode *parent;
   void *data[(1 << 4)];
} RadixTreeNode;

typedef struct RadixTreeRoot
{
   unsigned int height;
   SpinLock lock;
   RadixTreeNode *node;
} RadixTreeRoot;

inline int initRadixTreeRoot(RadixTreeRoot *root)
   __attribute__ ((always_inline));

inline int initRadixTreeRoot(RadixTreeRoot *root)
{
   root->height = 0;
   root->node = 0;
   initSpinLock(&root->lock);
   return 0;
}

int destoryRadixTreeRoot(RadixTreeRoot *root);

int insertIntoRadixTree(RadixTreeRoot *root,unsigned int index,void *item);
int removeFromRadixTree(RadixTreeRoot *root,unsigned int index);
void *getFromRadixTree(RadixTreeRoot *root,unsigned int index);
