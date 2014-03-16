#include <core/const.h>
#include <memory/buddy.h>
#include <memory/memory.h>
#include <video/console.h>
#include <cpu/spinlock.h>
#include <block/pagecache.h>

#define MAX_ORDER 0xB

extern void *endAddressOfKernel; /*See also ldscripts/kernel.lds.*/
                                 /*It will init in start/cstart.c.*/
static PhysicsPage *memoryMap;
static u64 physicsPageCount;
static u64 freePhysicsPageCount;

static ListHead buddyFreeList[MAX_ORDER];
/*MAX_ORDER is 11,so we can get 2^(MAX_ORDER - 1)*PAGE_SIZE = 4MB memory once at most.*/
static SpinLock buddySpinLock[MAX_ORDER];

static inline int initPhysicsPage(PhysicsPage *page) 
   __attribute ((always_inline));

static inline int pageIsBuddy(PhysicsPage *page,unsigned int order) 
   __attribute ((always_inline)); 

static inline int initPhysicsPage(PhysicsPage *page)
{
   atomicSet(&page->count,1); /*Used.*/
   page->flags = 0;
   page->data = 0;
   initList(&page->list);
   return 0;
}

static inline int pageIsBuddy(PhysicsPage *page,unsigned int order)
{
   if((atomicRead(&page->count) == 0) && !(page->flags & PageReserved) &&
      (page->flags & PageData) && (page->data == order))
      return 1;
   return 0;
}

int initBuddySystem(void)
{
   memoryMap = (PhysicsPage *)endAddressOfKernel;

   physicsPageCount = getMemorySize();
   physicsPageCount += 0xfff;
   physicsPageCount >>= 3*4;
   printk("Physics Page Count:0x%lx\n",physicsPageCount);

   for(int order  = 0;order < MAX_ORDER;++order)
   {
      initList(&buddyFreeList[order]);
   }
   for(int order = 0;order < MAX_ORDER;++order)
   {
      initSpinLock(&buddySpinLock[order]);
   }

   for(int i = 0;i < physicsPageCount;++i)
   {
      initPhysicsPage(memoryMap + i);
   }

   endAddressOfKernel += physicsPageCount * sizeof(PhysicsPage) + 1;
   printk("The last physics page address: 0x%p,the size of physics pages: %ldKB.\n",
      endAddressOfKernel,
      physicsPageCount * sizeof(PhysicsPage)/1024 + 1);
   freePhysicsPageCount = 0;

   printk("Initialize Buddy System successfully!\n");
   return 0;
}

int freePages(PhysicsPage *page,unsigned int order)
{
   int retval;
   u64 pageIndex = (u64)(page - memoryMap);
   u64 buddyIndex = 0;
   PhysicsPage *targetPage = 0;
   PhysicsPage *buddy = 0;
   if(pageIndex < 0)
      return -EINVAL; /*Error!*/
   if(page->flags & PageReserved)
      return -EPERM;
   if(page->flags & PagePageCache)
      return (*page->cache->operation->putPage)(page);
   if((retval = atomicAddRet(&page->count,-1)) != 0)
      return retval;
   freePhysicsPageCount += (1 << order);
   lockSpinLock(&buddySpinLock[order]);
   while(order < MAX_ORDER - 1)
   {
      buddyIndex = pageIndex ^ (1 << order);
      buddy = memoryMap + buddyIndex;
      if(!pageIsBuddy(buddy,order))
         break;
      page->flags = 0;
      page->data = 0;
      listDelete(&buddy->list);
      pageIndex &= buddyIndex;
      unlockSpinLock(&buddySpinLock[order]);
      ++order;
      lockSpinLock(&buddySpinLock[order]);
   } /*Try to merge.*/
   targetPage = memoryMap + pageIndex;
   targetPage->flags |= PageData;
   targetPage->data = order;
   atomicSet(&targetPage->count,0);
   listAddTail(&targetPage->list,&buddyFreeList[order]);
   unlockSpinLock(&buddySpinLock[order]);
   return retval;
}

PhysicsPage *allocPages(unsigned int order)
{
   unsigned int currentOrder = order;
   u64 size;
   PhysicsPage *page,*buddy;
   for(;currentOrder < MAX_ORDER;++currentOrder)
   {
      lockSpinLock(&buddySpinLock[currentOrder]);
      if(!listEmpty(&buddyFreeList[currentOrder]))
      {
         page = listEntry(buddyFreeList[currentOrder].next,PhysicsPage,list);
         listDelete(&page->list);
         unlockSpinLock(&buddySpinLock[currentOrder]);
         
         atomicAdd(&page->count,1);
         page->flags &= ~PageData;
         page->data = 0;
         size = 1ul << currentOrder;
         while(currentOrder > order)
         {
            --currentOrder;
            size >>= 1;
            buddy = page + size;
            lockSpinLock(&buddySpinLock[currentOrder]);
            listAddTail(&buddy->list,&buddyFreeList[currentOrder]);
            unlockSpinLock(&buddySpinLock[currentOrder]);
            buddy->flags |= PageData;
            buddy->data = currentOrder;
         } /*Split the page.*/
         freePhysicsPageCount -= (1 << order);
         return page;
      }
      unlockSpinLock(&buddySpinLock[currentOrder]);
   }
   return 0;
}

PhysicsPage *allocAlignedPages(unsigned int order)
{
   unsigned int currentOrder = order;
   u64 size;
   PhysicsPage *page,*buddy;
   u64 align = (PHYSICS_PAGE_SIZE << order) - 1;
   for(;currentOrder < MAX_ORDER;++currentOrder)
   {
      lockSpinLock(&buddySpinLock[currentOrder]);
      for(ListHead *list = buddyFreeList[currentOrder].next;list != &buddyFreeList[currentOrder];list = list->next)
      {
         page = listEntry(list,PhysicsPage,list);
         if(((u64)getPhysicsPageAddress(page) & align) != 0x0)
            continue;
         listDelete(&page->list);
         unlockSpinLock(&buddySpinLock[currentOrder]);
         atomicAdd(&page->count,1);
         page->flags &= ~PageData;
         page->data = 0;
         size = 1ul << currentOrder;
         while(currentOrder > order)
         {
            --currentOrder;
            size >>= 1;
            buddy = page + size;
            lockSpinLock(&buddySpinLock[currentOrder]);
            listAddTail(&buddy->list,&buddyFreeList[currentOrder]);
            unlockSpinLock(&buddySpinLock[currentOrder]);
            buddy->flags |= PageData;
            buddy->data = currentOrder;
         } /*Split the page.*/
         freePhysicsPageCount -= (1 << order);
         return page;
      }
      unlockSpinLock(&buddySpinLock[currentOrder]);
   }
   return 0;
}

PhysicsPage *allocDMAPages(unsigned int order,unsigned int max)
{
   unsigned int currentOrder = order;
   u64 size;
   PhysicsPage *page,*buddy;
   for(;currentOrder < MAX_ORDER;++currentOrder)
   {
      lockSpinLock(&buddySpinLock[currentOrder]);
      for(ListHead *list = buddyFreeList[currentOrder].next;list != &buddyFreeList[currentOrder];list = list->next)
      {
         page = listEntry(list,PhysicsPage,list);
         pointer address = va2pa(getPhysicsPageAddress(page));
         if((address & ((1ul << max) - 1)) != address)
            continue;
         listDelete(&page->list);
         unlockSpinLock(&buddySpinLock[currentOrder]);
         atomicAdd(&page->count,1);
         page->flags &= ~PageData;
         page->data = 0;
         size = 1ul << currentOrder;
         while(currentOrder > order)
         {
            --currentOrder;
            size >>= 1;
            buddy = page + size;
            lockSpinLock(&buddySpinLock[currentOrder]);
            listAddTail(&buddy->list,&buddyFreeList[currentOrder]);
            unlockSpinLock(&buddySpinLock[currentOrder]);
            buddy->flags |= PageData;
            buddy->data = currentOrder;
         } /*Split the page.*/
         freePhysicsPageCount -= (1 << order);
         return page;
      }
      unlockSpinLock(&buddySpinLock[currentOrder]);
   }
   return 0;
}

PhysicsPage *getMemoryMap(void)
{
   return memoryMap;
}

u64 getPhysicsPageCount(void)
{
   return physicsPageCount;
}

int dereferencePage(PhysicsPage *page,unsigned int order) __attribute__ ((alias("freePages")));
