#include <memory/buddy.h>
#include <memory/memory.h>
#include <video/console.h>

#define MAX_ORDER 0xB

extern void *endAddressOfKernel; /*See also ldscripts/kernel.lds.*/
                                 /*It will init in start/cstart.c.*/
static PhysicsPage *memoryMap = 0;
static u64 physicsPageCount = 0;

static ListHead freeList[MAX_ORDER] = {};
/*MAX_ORDER is 11,so we can get 2^(MAX_ORDER - 1)*PAGE_SIZE = 4MB memory once at most.*/

static inline int initPhysicsPage(PhysicsPage *page) 
   __attribute ((always_inline));

static inline int pageIsBuddy(PhysicsPage *page,unsigned int order) 
   __attribute ((always_inline)); 

static inline int initPhysicsPage(PhysicsPage *page)
{
   page->count = 1; /*Used.*/
   page->flags = 0;
   page->data = 0;
   initList(&page->list);
   return 0;
}

static inline int pageIsBuddy(PhysicsPage *page,unsigned int order)
{
   if((page->count == 0) && !(page->flags & PageReserved) &&
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
   printk("Physics Page Count:%x\n",physicsPageCount);

   for(int order  = 0;order < MAX_ORDER;++order)
   {
      initList(&freeList[order]);
   }

   for(int i = 0;i < physicsPageCount;++i)
   {
      initPhysicsPage(memoryMap + i);
   }

   endAddressOfKernel += physicsPageCount*sizeof(PhysicsPage) + 1;
   printk("The last physics page address: %x,the size of physics pages: %dKB.\n",
      endAddressOfKernel,
      physicsPageCount * sizeof(PhysicsPage)/1024 + 1);

   printk("Initialize Buddy System successfully!\n");
   return 0;
}

int freePages(PhysicsPage *page,unsigned int order)
{
   u64 pageIndex = (u64)(page - memoryMap);
   u64 buddyIndex = 0;
   PhysicsPage *targetPage = 0;
   PhysicsPage *buddy = 0;
   if(pageIndex < 0)
      return -EINVAL; /*Error!*/
   if(page->flags & PageReserved)
      return -EPERM;
   if(--page->count)
      return 0;
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
      ++order;
   } /*Try to merge.*/
   targetPage = memoryMap + pageIndex;
   targetPage->flags |= PageData;
   targetPage->data = order;
   targetPage->count = 0;
   listAddTail(&targetPage->list,&freeList[order]);
   return 0;
}

PhysicsPage *allocPages(unsigned int order)
{
   unsigned int currentOrder = order;
   u64 size;
   PhysicsPage *page,*buddy;
   for(;currentOrder < MAX_ORDER;++currentOrder)
   {
      if(!listEmpty(&freeList[currentOrder]))
      {
         page = listEntry(freeList[currentOrder].next,PhysicsPage,list);
         ++page->count;
         page->flags &= ~PageData;
         page->data = 0;
         listDelete(&page->list);
         size = 1ul << currentOrder;
         while(currentOrder > order)
         {
            --currentOrder;
            size >>= 1;
            buddy = page + size;
            listAddTail(&buddy->list,&freeList[currentOrder]);
            buddy->flags |= PageData;
            buddy->data = currentOrder;
         } /*Split the page.*/
         return page;
      }
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
      for(ListHead *list = freeList[currentOrder].next;list != &freeList[currentOrder];list = list->next)
      {
         page = listEntry(list,PhysicsPage,list);
         if(((u64)getPhysicsPageAddress(page) & align) != 0x0)
            continue;
         ++page->count;
         page->flags &= ~PageData;
         page->data = 0;
         listDelete(&page->list);
         size = 1ul << currentOrder;
         while(currentOrder > order)
         {
            --currentOrder;
            size >>= 1;
            buddy = page + size;
            listAddTail(&buddy->list,&freeList[currentOrder]);
            buddy->flags |= PageData;
            buddy->data = currentOrder;
         } /*Split the page.*/
         return page;
      }
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
      for(ListHead *list = freeList[currentOrder].next;list != &freeList[currentOrder];list = list->next)
      {
         page = listEntry(list,PhysicsPage,list);
         pointer address = va2pa(getPhysicsPageAddress(page));
         if((address & ((1ul << max) - 1)) != address)
            continue;
         ++page->count;
         page->flags &= ~PageData;
         page->data = 0;
         listDelete(&page->list);
         size = 1ul << currentOrder;
         while(currentOrder > order)
         {
            --currentOrder;
            size >>= 1;
            buddy = page + size;
            listAddTail(&buddy->list,&freeList[currentOrder]);
            buddy->flags |= PageData;
            buddy->data = currentOrder;
         } /*Split the page.*/
         return page;
      }
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
