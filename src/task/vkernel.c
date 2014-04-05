#include <core/const.h>
#include <filesystem/virtual.h>
#include <memory/buddy.h>
#include <string.h>

extern int __vkernelStartAddress(void);
extern int __vkernelSignalReturn(void);
extern int __vkernelSyscallEntry(void);
extern int __vkernelEndAddress(void);
      /*See vkernel.S .*/

static PhysicsPage *vkernelGetPage(VFSINode *inode,u64 offset);
static int vkernelPutPage(PhysicsPage *page);

static PhysicsPage *vkernelPage;
static VFSFile vkernel = 
{
   .dentry = &(struct VFSDentry)
   {
      .inode = &(struct VFSINode)
      {
         .cache = 
         {
            .operation = &(struct PageCacheOperation)
            {
               .getPage = &vkernelGetPage,
               .putPage = &vkernelPutPage
                  /*The get page and put page functions.*/
            }
         }
      }
   }
};
   /*The vkernel file.In fact,it is not exists in any file systems.*/

static PhysicsPage *vkernelGetPage(VFSINode *inode,u64 offset)
{
   return vkernelPage;
}

static int vkernelPutPage(PhysicsPage *page)
{
   return 0; /*We need not do anything here.*/
}

static int initVKernelPage(void)
{
   vkernelPage = allocPages(0);
   if(unlikely(!vkernelPage))
      return -ENOMEM;
   void *tmp = getPhysicsPageAddress(vkernelPage);
   memcpy((void *)tmp,(const void *)&__vkernelStartAddress,
           &__vkernelEndAddress - &__vkernelStartAddress);
           /*Copy the vkernel content to the vkernel page.*/

   atomicSet(&vkernel.ref,1);
   return 0;
}

void *vkernelSignalReturn(void *base)
{
   return base + (&__vkernelSignalReturn - &__vkernelStartAddress);
      /*It's base + [the offset of SignalReturn].*/
}

void *vkernelSyscallEntry(void *base)
{
   return base + (&__vkernelSyscallEntry - &__vkernelStartAddress);
      /*It's base + [the offset of Syscall Entry].*/
}

VFSFile *getVKernelFile(void)
{
   return &vkernel; /*Return the address of the vkernel file.*/
}

subsysInitcall(initVKernelPage);
