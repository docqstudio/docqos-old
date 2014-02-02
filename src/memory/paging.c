#include <core/const.h>
#include <memory/paging.h>
#include <memory/memory.h>
#include <memory/buddy.h>
#include <memory/kmalloc.h>
#include <lib/string.h>
#include <filesystem/virtual.h>
#include <task/semaphore.h>
#include <interrupt/interrupt.h>

#define MIN_MAPPING (1024ul*1024*1024*4) /*4GB.*/

extern void *endAddressOfKernel;
TaskMemory *taskForkMemory(TaskMemory *old,ForkFlags flags);
int taskExitMemory(TaskMemory *old);

static u64 *kernelPML4EDir;
static u64 *kernelPDPTEDir;
static u64 *kernelPDEDir;

static u64 pml4eCount;

extern int calcMemorySize(void);

static void *allocPML4E(void)
{
   PhysicsPage *page = allocPages(0);
   if(!page)
      return 0;
   u64 *ret = (u64 *)getPhysicsPageAddress(page);
   memset(ret,0,0x1000);
   for(int i = 1;i < pml4eCount;++i)
      ret[i] = ((u64)va2pa(&kernelPDPTEDir[(i - 1) * 512])) + 0x003;
         /*Set kernel pages.*/
   return ret;
}

static void *allocPDPTE(void *__pml4e,pointer address)
{
   u64 *pml4e = (u64 *)__pml4e;
   u64 nr = (address >> 39) & 0x1ff; /*39 - 47 bits.*/
   u64 data = pml4e[nr];
   if(data & 0x1) /*Exists?*/
      return pa2va(data & ~(0x1000 - 1));
   PhysicsPage *page = allocPages(0);
   if(!page)
      return 0;
   u64 *ret = (u64 *)getPhysicsPageAddress(page);
   memset(ret,0,0x1000); /*Zero.*/
   pml4e[nr] = ((u64)va2pa(ret)) + 0x007; /*P,R/W and U/S.*/
   return ret;
}

static void *allocPDE(void *__pdpte,pointer address)
{
   u64 *pdpte = (u64 *)__pdpte;
   u64 nr = (address >> 30) & 0x1ff; /*30 - 38 bits.*/
   u64 data = pdpte[nr];
   if(data & 0x1) /*Exists?*/
      return pa2va(data & ~(0x1000 - 1));
   PhysicsPage *page = allocPages(0);
   if(!page)
      return 0;
   u64 *ret = (u64 *)getPhysicsPageAddress(page);
   memset(ret,0,0x1000); /*Zero.*/
   pdpte[nr] = ((u64)va2pa(ret)) + 0x007; /*P,R/W and U/S.*/
   return ret;
}

static void *allocPTE(void *__pde,pointer address)
{
   u64 *pde = (u64 *)__pde;
   u64 nr = (address >> 21) & 0x1ff; /*21 - 29 bits.*/
   u64 data = pde[nr];
   if(data & 0x1) /*Exists?*/
      return pa2va(data & ~(0x1000 - 1));
   PhysicsPage *page = allocPages(0);
   if(!page)
      return 0;
   u64 *ret = (u64 *)getPhysicsPageAddress(page);
   memset(ret,0,0x1000); /*Zero.*/
   pde[nr] = ((u64)va2pa(ret)) + 0x007; /*P,R/W and U/S.*/
   return ret;
}

static int setPTE(void *__pte,pointer address,pointer p)
{
   u64 *pte = (u64 *)__pte;
   u64 nr = (address >> 12) & 0x1ff;
   u64 data = pte[nr];
   if(data & 0x1) /*It has set?*/
      return -EBUSY;
   pte[nr] = ((u64)p) + 0x007; /*P,R/W and U/S.*/
   return 0;
}

static int freePTE(void *__pte,int (*pteFreer)(void *data))
{
   u64 *pte = (u64 *)__pte;
   for(int i = 0;i < 512;++i)
   {
      if(!(pte[i] & 0x1)) /*Exist?*/
         continue;
      (*pteFreer)(pa2va(pte[i] & ~(0x1000 - 1)));
   }
   freePages(getPhysicsPage(pte),0);
   return 0;
}

static int freePDE(void *__pde,int (*pteFreer)(void *data))
{
   u64 *pde = (u64 *)__pde;
   for(int i = 0;i < 512;++i)
   {
      if(!(pde[i] & 0x1)) /*Exist?*/
         continue;
      freePTE(pa2va(pde[i] & ~(0x1000 - 1)),pteFreer);
   }
   freePages(getPhysicsPage(pde),0);
   return 0;
}

static int freePDPTE(void *__pdpte,int (*pteFreer)(void *data))
{
   u64 *pdpte = (u64 *)__pdpte;
   for(int i = 0;i < 512;++i)
   {
      if(!(pdpte[i] & 0x1)) /*Exist?*/
         continue;
      freePDE(pa2va(pdpte[i] & ~(0x1000 - 1)),pteFreer);
   }
   freePages(getPhysicsPage(pdpte),0);
   return 0;
}

static int freePML4E(void *__pml4e,int (*pteFreer)(void *data))
{
   u64 *pml4e = (u64 *)__pml4e;
   if(!(pml4e[0] & 0x1)) /*Exist?*/
      return 0;
   freePDPTE(pa2va(pml4e[0] & ~(0x1000 - 1)),pteFreer);
   freePages(getPhysicsPage(pml4e),0);
   return 0;
}

static int mmapPTEFreer(void *data)
{
   return freePages(getPhysicsPage(data),0);
}

static void *copyPTE(void *pde,int nr,
                     void *__pte,void *(*pteCopier)(void *data))
{
   u64 *pte = (u64 *)__pte;
   void *new = allocPTE(pde,(pointer)nr << 21);
   for(int i = 0;i < 512;++i)
   {
      if(!(pte[i] & 0x1)) /*Exist?*/
         continue;
      void *n = (*pteCopier)(pa2va(pte[i] & ~(0x1000 - 1)));
      if(!n)
         return 0;
      setPTE(new,(pointer)i << 12,va2pa(n));
   }
   return new;
}

static void *copyPDE(void *pdpte,int nr,
                     void *__pde,void *(*pteCopier)(void *data))
{
   u64 *pde = (u64 *)__pde;
   void *new = allocPDE(pdpte,(pointer)nr << 30);
   for(int i = 0;i < 512;++i)
   {
      if(!(pde[i] & 0x1)) /*Exist?*/
         continue;
      void *n = copyPTE(new,i,pa2va(pde[i] & ~(0x1000 - 1)),pteCopier);
      if(!n)
         return 0;
   }
   return new;
}

static void *copyPDPTE(void *pml4e,int nr,
                       void *__pdpte,void *(*pteCopier)(void *data))
{
   u64 *pdpte = (u64 *)__pdpte;
   void *new = allocPDPTE(pml4e,(pointer)nr << 39);
   for(int i = 0;i < 512;++i)
   {
      if(!(pdpte[i] & 0x1)) /*Exist?*/
         continue;
      void *n = copyPDE(new,i,pa2va(pdpte[i] & ~(0x1000 - 1)),pteCopier);
      if(!n)
         return 0; /*Returning zero is OK.*/
         /*Because new has been set in pml4e since we called allocPDPTE.*/
         /*And pml4e will be destoried soon!!!!*/
   }
   return new;
}

static void *copyPML4E(void *__pml4e,void *(*pteCopier)(void *data),u8 *failed)
{
   u64 *pml4e = (u64 *)__pml4e;
   void *new = allocPML4E();
   if(!new || !pml4e) /*If pml4e is nullptr,just return allocPML4E().*/
      return new;
   if(!(pml4e[0] & 0x1))
      return new;
   void *n = copyPDPTE(new,0,pa2va(pml4e[0] & ~(0x1000 - 1)),pteCopier);
   if(!n)
   {
      if(failed)
         *failed = 1;
      return new;
   }
   if(failed)
      *failed = 0;
      /*pml4e[0]:user page tables*/
      /*pml4e[1]:kernel page tables,always kernelPDPTEDir*/
      /*pml4e[2] ~ pml4e[511]:unused*/
   return new;
}

static void *mmapPTECopier(void *data)
{
   PhysicsPage *page = allocPages(0);
   void *new = getPhysicsPageAddress(page);
   memcpy((void *)new,(const void *)data,0x1000);
   return new;
}

static VirtualMemoryArea *lookForVirtualMemoryArea(
                          TaskMemory *mm,u64 start)
{ /*This function looks for the last VirtulMemoryArea that is before 'strart'.*/
  /*This function will return 0 when 1. mm->vm == 0 .*/
  /*                                 2. mm->vm->start + mm->vm->length > start .*/
   VirtualMemoryArea *vma = mm->vm;
   VirtualMemoryArea *prev = 0;
   while(vma)
   {
      if(vma->start + vma->length > start)
         break;
      prev = vma;
      vma = vma->next;
   }
   return prev;
}

static u64 lookForFreeVirtualMemoryArea(
              TaskMemory *mm,u64 start,u64 length,VirtualMemoryArea **v)
{
   if(length > PAGE_OFFSET)
      return -ENOMEM; /*Too large!*/
   VirtualMemoryArea *vma;
   if(start && start + length <= PAGE_OFFSET)
   {
      vma = lookForVirtualMemoryArea(mm,start);
      if(!vma || !vma->next)
         goto found;
      if(vma->next->start >= start + length)
         goto found;
   }
   start = 1024 * 1024 * 1024; /*Look from 1GB.*/
   for(vma = lookForVirtualMemoryArea(mm,start);
          start + length <= PAGE_OFFSET;vma = vma->next)
   {
      if(!vma || !vma->next)
         goto found;
      if(vma->next->start >= start + length) /*Is there enough free area?*/
         goto found;
      start = vma->next->start + vma->next->length;
   }
   return -ENOMEM;
found:
   if(v)
      *v = vma;/*Save it to *v .*/
   return start;
}

int initPaging(void)
{
   u64 mappingSize = getMemorySize();
   if(mappingSize < MIN_MAPPING)
      mappingSize = MIN_MAPPING;
   
   u64 pdeCount = mappingSize / (1024 * 1024 * 2); /*2MB.*/
   if(mappingSize % (1024 * 1024 * 2) != 0)
      pdeCount += 1;   /*Round up.*/
   
   u64 pdpteCount = pdeCount / 512;
   if(pdeCount % 512 != 0)
      pdpteCount += 1; /*Round up.*/
   
   pml4eCount = pdpteCount / 512;
   if(pdpteCount % 512 != 0)
      pml4eCount += 1; /*Round up.*/
   
   u64 *endAddress;

   ++pml4eCount; /*Reserved for task.*/

#define ALIGN(p) ((p + 511) & (~511))
   u64 realPDECount = ALIGN(pdeCount);
   u64 realPDPTECount = ALIGN(pdpteCount);
   u64 realPML4ECount = ALIGN(pml4eCount);
#undef ALIGN

#define ALIGN(p) ((p + 4095) & (~4095))

   /*Init kernel PDE page table.*/
   kernelPDEDir = (u64 *)ALIGN((pointer)endAddressOfKernel);
   for(int i = 0;i < pdeCount;++i)
   {
      kernelPDEDir[i] = ((u64)i) * (1024 * 1024 * 2) + 0x183;
                                /*All kernel pages are global pages.*/
   }
   for(int i = pdeCount;i < realPDECount;++i)
   {
      kernelPDEDir[i] = 0;
   }
   endAddress = &kernelPDEDir[realPDECount];

   /*Init kernel PDPTE page table.*/
   kernelPDPTEDir = (u64 *)ALIGN((pointer)endAddress);
   for(int i = 0;i < pdpteCount;++i)
   {
      kernelPDPTEDir[i] = ((u64)va2pa(&kernelPDEDir[i * 512])) + 0x003;
   }
   for(int i = pdpteCount;i < realPDPTECount;++i)
   {
      kernelPDPTEDir[i] = 0;
   }
   endAddress = &kernelPDPTEDir[realPDPTECount];

   /*Init kernel PML4E page table.*/
   kernelPML4EDir = (u64 *)ALIGN((pointer)endAddress);
   for(int i = 1;i < pml4eCount;++i)
   {
      kernelPML4EDir[i] = ((u64)va2pa(&kernelPDPTEDir[(i - 1) * 512])) + 0x003;
   }
   for(int i = pml4eCount;i < realPML4ECount;++i)
   {
      kernelPML4EDir[i] = 0;
   }
   endAddress = &kernelPML4EDir[realPML4ECount] + 1;

   /*Update endAddressOfKernel.*/
   endAddressOfKernel = (void *)endAddress;

#undef ALIGN

   asm volatile("movq %%rax,%%cr3"::"a" (va2pa(kernelPML4EDir)):"memory");
   return 0;
}


int doMMap(VFSFile *file,u64 offset,pointer address,u64 len)
{
   Task *current = getCurrentTask();
   TaskMemory *mm = current->mm;
   VirtualMemoryArea *vma = 0;
   u64 start = lookForFreeVirtualMemoryArea(mm,address,len,&vma);

   if((s64)start < 0)
      return (s64)start;
   if(address && address != start)
      return -EBUSY;
   VirtualMemoryArea *new = kmalloc(sizeof(*new));
   if(!new)
      return -ENOMEM;
   new->start = start;
   new->length = len;
   new->file = file;
   new->offset = offset;
   if(vma)
   {
      new->next = vma->next;
      vma->next = new;
      return 0;
   }
   new->next = mm->vm;
   mm->vm = new;
   return 0;
}

int taskSwitchMemory(TaskMemory *old,TaskMemory *new)
{
   if(!new || !new->page)
      return 0;
   if(old && (new->page == old->page))
      return 0;
   asm volatile("movq %%rax,%%cr3"::"a"(va2pa(new->page)));
                 /*Switch %cr3.*/
   return 0;
}

TaskMemory *taskForkMemory(TaskMemory *old,ForkFlags flags)
{
   if(flags & ForkKernel)
      return 0; /*Kernel task has no TaskMemory.*/
   if(flags & ForkShareMemory)
   {
      if(!old)
         return old;
      atomicAdd(&old->ref,1);
      return old;
   }
   TaskMemory *new = kmalloc(sizeof(*new));
   atomicSet(&new->ref,1);
   new->page = 0;
   new->wait = 0;
   new->exec = 0;
   new->vm = 0;

   u8 failed = 0;
   new->page = copyPML4E(old ? old->page : 0,&mmapPTECopier,&failed);
   if(failed || !new->page) /*If failed,return zero.*/
   {
      if(new->page)
         freePML4E(new->page,&mmapPTEFreer);
      kfree(new);
      return 0;
   }
   if(old)
   {
      new->exec = cloneFile(old->exec);
      if(old->exec && !new->exec)
      {
         taskExitMemory(new);
         return 0;
      }
      VirtualMemoryArea *vma,*nvma,**pvma;
      vma = old->vm;
      pvma = &new->vm;
      while(vma)
      {
         nvma = kmalloc(sizeof(*nvma));
         if(!nvma)
         {
            taskExitMemory(new);
            return 0;
         }
         nvma->offset = vma->offset;
         if(!vma->file)
            nvma->file = 0;
         else if(vma->file == old->exec)
            nvma->file = new->exec;
         else
            nvma->file = cloneFile(vma->file);
         nvma->start = vma->start;
         nvma->length = vma->length;
         nvma->next = 0;

         *pvma = nvma;
         pvma = &nvma->next;
         vma = vma->next;
      }
   }
   return new;
}

int taskExitMemory(TaskMemory *old)
{
   int ref = atomicAddRet(&old->ref,-1);
   Semaphore *wait;
   VirtualMemoryArea *vma,*__vma;
   switch(ref)
   {
   case 1:
      wait = old->wait;
      old->wait = 0;
      if(wait)
         upSemaphore(wait); /*TaskMemory's wait field is for vfork system call.*/
      break;
   case 0:
      freePML4E(old->page,&mmapPTEFreer);
      vma = old->vm;
      while(vma)
      {
         __vma = vma->next;
         if(vma->file && vma->file != old->exec)
            closeFile(vma->file);
         kfree(vma);
         vma = __vma;
      }
      if(old->exec)
         closeFile(old->exec);
      kfree(old);
      break;
   default:
      break;
   }
   return 0;
}

int doPageFault(IRQRegisters *reg)
{
   u64 address,pos = 0;
   VirtualMemoryArea *vma;
   Task *current = getCurrentTask();
   int retval;

   asm volatile("movq %%cr2,%%rax":"=a"(address));
                      /*Get the address which produces this exception.*/
   if(!current || !current->mm) /*Kernel Task!*/
      return -ENOSYS;
   if(address > PAGE_OFFSET)
      return -EFAULT;
   vma = lookForVirtualMemoryArea(current->mm,address);
   if(!vma && current->mm->vm)
      vma = current->mm->vm;
   else if(!vma || !vma->next)
      return -EFAULT;
   else
      vma = vma->next;

   if(vma->start > address) /*Is the address in the VirtualMemoryArea?*/
      return -EFAULT;

   VFSFile *file = vma->file;
   if(file)
   {
      pos = address & ~0xfff;
      pos -= vma->start;
      if(lseekFile(file,pos > 0 ? pos : -pos))
         return -EIO;
   }

   void *pml4e = current->mm->page;
   if(!pml4e)
      current->mm->page = pml4e = allocPML4E();
   if(!pml4e)
      return -ENOMEM;
   void *pdpte = allocPDPTE(pml4e,address);
   if(!pdpte)
      return -ENOMEM;
   void *pde = allocPDE(pdpte,address);
   if(!pde)
      return -ENOMEM;
   void *pte = allocPTE(pde,address);
   if(!pte)
      return -ENOMEM; /*Alloc page tables.*/

   void *page = allocPages(0);
   if(!page)
      return -ENOMEM;
   page = getPhysicsPageAddress((PhysicsPage *)page);
   retval = -EIO;
   if(file)
      if(readFile(file,page + ((pos < 0) ? -pos : 0),0x1000 - ((pos < 0) ? -pos : 0)) < 0)
         goto failed; /*Read data from the file!*/
   retval = -EBUSY;
   if(setPTE(pte,address,va2pa(page)) < 0)
      goto failed;

   asm volatile("movq %%rax,%%cr3"::"a"(va2pa(pml4e))); /*Refresh the TLBs.*/
   return 0;
failed:
   freePages(getPhysicsPage(page),0);
   return retval;
}
