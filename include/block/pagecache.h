#pragma once
#include <core/const.h>
#include <cpu/radixtree.h>

typedef struct VFSINode VFSINode;
typedef struct PhysicsPage PhysicsPage;

typedef struct PageCacheOperation
{
   PhysicsPage *(*getPage)(VFSINode *inode,u64 offset);
   int (*flushPage)(PhysicsPage *page);
   int (*putPage)(PhysicsPage *page);
} PageCacheOperation;

typedef struct PageCache
{
   VFSINode *inode;
   PageCacheOperation *operation;
   RadixTreeRoot radix;
} PageCache;

      /*Before call these functions below,we'd better downSemaphore(&cache->inode->semaphore).*/
PhysicsPage *getPageFromPageCache(PageCache *cache,unsigned int index,
         int (*readpage)(VFSINode *inode,PhysicsPage *page,unsigned int index));

int putPageIntoPageCache(PhysicsPage *page);
