#include <core/const.h>
#include <cpu/radixtree.h>
#include <memory/kmalloc.h>
#include <lib/string.h>

static inline unsigned int getRadixTreeMaxIndex(unsigned int height)
{
   height *= 4;
   height = 1 << height;
   height -= 1; /*Get the max index of a radix tree.*/
   return height;
}

static RadixTreeNode *createRadixTreeNode(RadixTreeNode *parent,void **item,int nr)
{
   RadixTreeNode *node = kmalloc(sizeof(*node));
   if(!node)
      return 0;
   memset(node->data,0,sizeof(node->data)); /*Set to zero.*/
   node->count = 0; /*No data!*/
   node->parent = parent;
   node->nr = nr;
   *item = node; /*Save the node to *item.*/
   if(parent)
      ++parent->count; /*Add the count.*/
   return node;
}

static int extandRadixTree(RadixTreeRoot *root,unsigned int index)
{
   unsigned int height = root->height + 1;
   while(index >= getRadixTreeMaxIndex(height))
      ++height;
   if(!root->node && (root->height = height))
      return 0; /*Just return!*/

   do{
      RadixTreeNode *old = root->node;
      RadixTreeNode *node = createRadixTreeNode(0,(void **)&root->node,0);
      if(!node)
         return -ENOMEM;
      node->data[0] = old; /*First!*/
      old->nr = 0;
   } while(height > root->height);
   return 0;
}

static int destoryRadixTreeNode(RadixTreeRoot *root,RadixTreeNode *node,int height)
{
   if(height == root->height)
      goto out;
   for(int i = 0;i < sizeof(node->data) / sizeof(node->data[0]);++i)
      if(node->data[i]) /*The data exists?*/
         destoryRadixTreeNode(root,node->data[i],height + 1); /*Destory it!*/
out:
   return kfree(node);
}

int destoryRadixTreeRoot(RadixTreeRoot *root)
{
   if(root->node)
      return destoryRadixTreeNode(root,root->node,1);
   else
      return 0; /*Nothing to do.*/
}

int insertIntoRadixTree(RadixTreeRoot *root,unsigned int index,void *item)
{
   lockSpinLock(&root->lock);

   int retval = -ENOMEM;
   unsigned int max = getRadixTreeMaxIndex(root->height);
   unsigned int height;
   RadixTreeNode **node = &root->node,*parent = 0;
   if(max <= index)
      if((retval = extandRadixTree(root,index)))
         goto out;
   height = root->height;

   for(int i = 0;i < height;++i)
   {
      int nr = (index >> (i * 4)) & 0xf;
      if(!*node && parent)
         createRadixTreeNode(parent,(void **)node,nr);
      if(!*node)
         goto out; /*OOM,out of memory.*/
      parent = *node;
      node = (typeof(node))&(*node)->data[nr];
   }
   retval = -EBUSY;
   if(*node)
      goto out; /*It has been set.*/
   ++parent->count;
   *node = item;
   retval = 0;
out:
   unlockSpinLock(&root->lock);
   return retval;
}

int removeFromRadixTree(RadixTreeRoot *root,unsigned int index)
{
   lockSpinLock(&root->lock);

   RadixTreeNode **node = &root->node,*parent = 0,*free = 0;
   int retval = -ENOENT;
   unsigned int height = root->height;
   unsigned int nr;
   if(getRadixTreeMaxIndex(height) <= index)
      goto out;

   for(int i = 0;i < height;++i)
   {
      int nr = (index >> (i * 4)) & 0xf;
      if(!*node)
         goto out; /*No such item.*/
      parent = *node;
      node = (typeof(node))&(*node)->data[nr];
   }
   *node = 0;

   do{
      if(free)
         (parent->data[nr] = 0),kfree(free);
      if(!--parent->count) /*It isn't used.*/
         (free = parent),(nr = parent->nr);
      else if((free = 0) || 1)
         break;
      parent = parent->parent;
   }while(parent);

   if(free)
      kfree(root->node),root->node = 0;

   retval = 0;
out:
   unlockSpinLock(&root->lock);
   return retval;
}

void *getFromRadixTree(RadixTreeRoot *root,unsigned int index)
{
   lockSpinLock(&root->lock);

   RadixTreeNode **node = &root->node;
   void *retval = 0;
   unsigned int height = root->height;
   if(getRadixTreeMaxIndex(height) <= index)
      goto out; /*No such item!*/

   for(int i = 0;i < height;++i)
   {
      int nr = (index >> (i * 4)) & 0xf;
      if(!*node)
         goto out;
      node = (typeof(node))&(*node)->data[nr];
   }
   retval = *node; /*Get the item.*/
out:
   unlockSpinLock(&root->lock);
   return retval;
}
