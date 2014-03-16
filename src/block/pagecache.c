#include <core/const.h>
#include <block/pagecache.h>
#include <memory/buddy.h>
#include <filesystem/virtual.h>

PhysicsPage *getPageFromPageCache(PageCache *cache,unsigned int index,
         int (*readpage)(VFSINode *cache,PhysicsPage *page,unsigned int index))
{
   VFSINode *inode = cache->inode;
   PhysicsPage *page = getFromRadixTree(&cache->radix,index);
   if(page)
      return referencePage(page); /*Reference the page.*/

   page = allocPages(0); /*Alloc a new page.*/
   if(!page)
      return 0;
   page->flags |= PagePageCache | PageData;
   page->cache = cache;
   page->data = index;
   (*readpage)(inode,page,index); /*Read the page!*/
   insertIntoRadixTree(&cache->radix,index,page);
   referencePage(page); /*Insert it into radix tree and reference the page.*/
   return page;
}

int putPageIntoPageCache(PhysicsPage *page)
{
   if(unlikely(!(page->flags & PagePageCache)))
      return -EINVAL;
   if(unlikely(!(page->flags & PageData)))
      return -EINVAL; /*It should never happen!!!!*/
   PageCache *cache = page->cache;
   unsigned int index = page->data;
   int retval;

   if((retval = atomicAddRet(&page->count,-1)) != 1)
      return retval; 

   removeFromRadixTree(&cache->radix,index);
   page->flags &= ~(PagePageCache | PageData);
   freePages(page,0); /*No one is using the page,free it.*/
   return 0;
}
