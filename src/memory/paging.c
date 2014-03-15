#include <core/const.h>
#include <core/math.h>
#include <memory/paging.h>
#include <memory/memory.h>
#include <memory/buddy.h>
#include <memory/kmalloc.h>
#include <lib/string.h>
#include <filesystem/virtual.h>
#include <task/semaphore.h>
#include <interrupt/interrupt.h>
#include <video/console.h>

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

   referencePage(getPhysicsPage(pml4e));
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

   referencePage(getPhysicsPage(pdpte));
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

   referencePage(getPhysicsPage(pde));
   return ret;
}

static int setPTEEntry(void *__pte,pointer address,pointer p)
{
   u64 *pte = (u64 *)__pte;
   u64 nr = (address >> 12) & 0x1ff;
   u64 data = pte[nr];
   
   pte[nr] = ((u64)p) + 0x007; /*P,R/W and U/S.*/

   if(!(data & 1))
      referencePage(getPhysicsPage(pte));
      /*Add the reference count.*/
   return 0;
}

static void *getPDPTE(void *__pml4e,pointer address)
{
   u64 *pml4e = __pml4e;
   u64 nr = (address >> 39) & 0x1ff; /*39 - 48 bits.*/
   u64 data = pml4e[nr];
   if(!(data & 0x1))
      return 0; /*It doesn't exist.*/
   return pa2va(data & ~(0x1000 - 1));
}

static void *getPDE(void *__pdpte,pointer address)
{
   u64 *pdpte = __pdpte;
   u64 nr = (address >> 30) & 0x1ff; /*30 - 39 bits.*/
   u64 data = pdpte[nr];
   if(!(data & 0x1))
      return 0; /*It doesn't exist.*/
   return pa2va(data & ~(0x1000 - 1));
  
}

static void *getPTE(void *__pde,pointer address)
{
   u64 *pde = __pde;
   u64 nr = (address >> 21) & 0x1ff; /*21 - 30 bits.*/
   u64 data = pde[nr];
   if(!(data & 0x1))
      return 0; /*It doesn't exist.*/
   return pa2va(data & ~(0x1000 - 1));
}

static void *getPTEEntry(void *__pte,pointer address)
{
   u64 *pte = __pte;
   u64 nr = (address >> 12) & 0x1ff; /*12 - 21 bits.*/
   u64 data = pte[nr];
   if(!(data & 0x1))
      return 0; /*It doesn't exist.*/
   return pa2va(data & ~(0x1000 - 1));
}

static int clearPTEEntry(void *__pml4e,void *__pdpte,void *__pde,void *__pte,pointer address)
{
   u64 *pml4e = __pml4e;
   u64 *pdpte = __pdpte;
   u64 *pde = __pde;
   u64 *pte = __pte;
   u64 nr0 = (address >>= 12) & 0x1ff;
   u64 nr1 = (address >>= 9) & 0x1ff;
   u64 nr2 = (address >>= 9) & 0x1ff;
   u64 nr3 = (address >>= 9) & 0x1ff;

   if(!(pte[nr0] & 0x1))
      return -ENOENT;
   pte[nr0] = 0;  /*Clear the PTE Entry.*/
   if(dereferencePage(getPhysicsPage(pte),0) > 0)
      return 0;
   pde[nr1] = 0; /*Clear the PDE Entry.*/
   if(dereferencePage(getPhysicsPage(pde),0) > 0)
      return 0;
   pdpte[nr2] = 0; /*Clear the PDPTE Entry.*/
   if(dereferencePage(getPhysicsPage(pdpte),0) > 0)
      return 0;
   pml4e[nr3] = 0; /*Clear the PML4E Entry.*/
   return dereferencePage(getPhysicsPage(pml4e),0);
}

static int setPTEEntryAttribute(void *__pte,pointer address,u8 write)
{
   u64 nr = (address >> 12) & 0x1ff;
   u64 *pte = __pte;
   u64 data = pte[nr];
   if(!(data & 1))
      return -ENOENT;
   data &= ~0xffful;
   data += write ? 0x07 : 0x05;
      /*R/W U/S P : 0x7 .*/
      /*U/S P : 0x5 .*/
   pte[nr] = data; 
   return 0;
}

static VirtualMemoryArea *lookForVirtualMemoryArea(
                          TaskMemory *mm,u64 start)
{ /*This function looks for the last VirtulMemoryArea that is before 'start'.*/
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

static int __doMUNMap(TaskMemory *mm,VirtualMemoryArea **pvma)
{
   VirtualMemoryArea *vma = *pvma;
   u64 end = vma->start + vma->length;
   void *pml4e,*pdpte,*pde,*pte;
   pml4e = mm->page;

   for(u64 address = vma->start & (0x1fful << 39);
            address < min(1ul << 48,end);
            address += 1ul << 39)
   {
      pdpte = getPDPTE(pml4e,address);
      if(!pdpte)
         continue;
      for(u64 offset0 = vma->start & (0x1fful << 30);
                offset0 < min(1ul << 39,end - address);
                offset0 += 1ul << 30)
      {
         pde = getPDE(pdpte,offset0);
         if(!pde)
            continue;
         for(u64 offset1 = vma->start & (0x1fful << 21);
                 offset1 < min(1ul << 30,end - address - offset0);
                 offset1 += 1ul << 21)
         {
            pte = getPTE(pde,offset1);
            if(!pte)
               continue;
            for(u64 offset2 = vma->start & (0x1fful << 12);
                     offset2 < min(1ul << 21,end - address - offset0 - offset1);
                     offset2 += 1ul << 12)
            { /*Foreach the PTE Entries!*/
               void *entry = getPTEEntry(pte,offset2);
               if(!entry)
                  continue;
               dereferencePage(getPhysicsPage(entry),0);
               clearPTEEntry(pml4e,pdpte,pde,pte,offset0 + offset1 + offset2 + address);
                    /*Clear the PTE Entry.*/
            }
         }
      }
   }
   *pvma = vma->next; /*Remove vma from the VirtualMemoryArea list.*/
   if(vma->file)
      vfsPutFile(vma->file);
   kfree(vma); /*Free the struct!*/
   return 0;
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
   endAddress = &kernelPML4EDir[realPML4ECount];

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
      return -EBUSY; /*In fact,we should still map it!*/
   VirtualMemoryArea *new = kmalloc(sizeof(*new));
   if(!new) /*Alloc the VirtualMemoryArea.*/
      return -ENOMEM;
   new->start = start;
   new->length = len;
   new->file = vfsGetFile(file);
   new->offset = offset; /*Set the fields.*/
   if(vma)
   {
      new->next = vma->next;
      vma->next = new;
      return 0;
   } /*Insert vma to current->mm->vm.*/
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
      return old; /*Share the struct.*/
   }
   TaskMemory *new = kmalloc(sizeof(*new));
   atomicSet(&new->ref,1);
   new->page = allocPML4E(); /*Alloc the PML4E page tables.*/
   new->wait = 0;
   new->exec = 0;
   new->vm = 0;
   if(!new->page && (kfree(new) || 1))
      return 0; /*OOM,Out Of Memory.*/

   void *opml4e,*opdpte,*opde,*opte,*oentry;
   void *pml4e,*pdpte,*pde,*pte;
   pml4e = pdpte = pde = pte = 0;
   if(!old)
      return new; /*We needn't copy page tables.*/
   if(old->exec)
      new->exec = vfsGetFile(old->exec); /*Add the reference count.*/
   if(!old->page)
      return new;
   pml4e = new->page;
   opml4e = old->page;
   
   VirtualMemoryArea *vma,*nvma,**pvma;
        /*The values: vma => the VirtualMemoryArea that we are copying from..*/
        /*            nvma => the VirtualMemoryArea that we are copying to.*/
        /*            pvma => the pointer to the previous VirtualMemoryArea's next field.*/
   vma = old->vm;
   pvma = &new->vm;
   while(vma)
   {
      nvma = kmalloc(sizeof(*nvma));
      if(!nvma)
      {
         taskExitMemory(new);
         return 0; /*No memory!*/
      }
      nvma->offset = vma->offset;
      if(!vma->file)
         nvma->file = 0;
      else
         nvma->file = vfsGetFile(vma->file);
      nvma->start = vma->start;
      nvma->length = vma->length;
      nvma->next = 0;

      u64 end = vma->start + vma->length;
      for(u64 address = vma->start & (0x1fful << 39);
               address < min(1ul << 48,end);
               address += 1ul << 39) /*Foreach the PML4E Table.*/
      {
         opdpte = getPDPTE(opml4e,address); /*Get the PDPTE Table from PML4E Table.*/
         if(!opdpte)
            continue;
         for(u64 offset0 = vma->start & (0x1fful << 30);
               offset0 < min(1ul << 39,end - address);
               offset0 += 1ul << 30) /*Foreach the PDPTE Table.*/
         {
            opde = getPDE(opdpte,offset0); /*Get the PDE Table from PDPTE Table.*/
            if(!opde)
               continue;
            for(u64 offset1 = vma->start & (0x1fful << 21);
                  offset1 < min(1ul << 30,end - address - offset0);
                  offset1 += 1ul << 21) /*Foreach the PDE Table.*/
            {
               opte = getPTE(opde,offset1); /*Get the PTE Table frome PDE Table.*/
               if(!opte)
                  continue;
               for(u64 offset2 = vma->start & (0x1fful << 12);
                     offset2 < min(1ul << 21,end - address - offset0 - offset1);
                     offset2 += 1ul << 12) /*Foreach the PTE Table.*/
               {
                  oentry = getPTEEntry(opte,offset2); 
                             /*Get the PTE Entry from PTE Table.*/
                  if(!oentry)
                     continue;
                  if(!pdpte)
                     pdpte = allocPDPTE(pml4e,address);
                  if(!pdpte)
                     goto failed;
                  if(!pde)
                     pde = allocPDE(pdpte,offset0);
                  if(!pde)
                     goto failed;
                  if(!pte)
                     pte = allocPTE(pde,offset1);
                  if(!pte)
                     goto failed;

                  setPTEEntry(pte,offset2,va2pa(oentry));
                  setPTEEntry(opte,offset2,va2pa(oentry));
                          /*Set the entry to the new page tables.*/
                  setPTEEntryAttribute(pte,offset2,0);
                  setPTEEntryAttribute(opte,offset2,0);
                           /*Now we clear the R/W bit!*/
                  referencePage(getPhysicsPage(oentry));
                          /*Add 1 to the reference count.*/
               }
               pte = 0;
            }
            pde = 0;
         }
         pdpte = 0;
      }
      *pvma = nvma;
      pvma = &nvma->next;
      vma = vma->next; /*Next VirtualMemoryArea struct.*/
   }
   return new;
failed:
   taskExitMemory(new);
   return 0;
}

int taskExitMemory(TaskMemory *old)
{
   int ref = atomicAddRet(&old->ref,-1);
   Semaphore *wait;
   switch(ref)
   {
   case 1:
      wait = old->wait;
      old->wait = 0;
      if(wait)
         upSemaphore(wait); /*TaskMemory's wait field is for vfork system call.*/
      break;
   case 0:
      while(old->vm)
         __doMUNMap(old,&old->vm);
         /*Unmap all virtual memory areas.*/
      if(old->exec)
         vfsPutFile(old->exec);
      kfree(old);
      break;
   default:
      break;
   }
   return 0;
}

int doPageFault(IRQRegisters *reg)
{
   u64 address,pos,base;
   VirtualMemoryArea *vma;
   Task *current = getCurrentTask();
   int retval;
   void *pml4e,*pdpte,*pde,*pte;
   void *entry,*data;
   PhysicsPage *entryPage,*dataPage;

   asm volatile("movq %%cr2,%%rax":"=a"(address));
                      /*Get the address which produces this exception.*/
   if(!current || !current->mm) /*Kernel Task!*/
      return -ENOSYS;
   if(address > PAGE_OFFSET)
      return -EFAULT;
   vma = lookForVirtualMemoryArea(current->mm,address);
                 /*Look for the virtual memory area!*/
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
      base = vma->offset + (pos > 0 ? pos : -pos);
   }

   pml4e = current->mm->page;
   if(!pml4e)
      current->mm->page = pml4e = allocPML4E();
   if(!pml4e)
      return -ENOMEM;
   pdpte = allocPDPTE(pml4e,address);
   if(!pdpte)
      return -ENOMEM;
   pde = allocPDE(pdpte,address);
   if(!pde)
      return -ENOMEM;
   pte = allocPTE(pde,address);
   if(!pte)
      return -ENOMEM; /*Alloc page tables.*/

   if(reg->irq & 1 /*Exist?*/)
      goto cow; /*Copy On Write.*/

   dataPage = allocPages(0);
   if(!dataPage)
      return -ENOMEM;
   data = getPhysicsPageAddress(dataPage);
   retval = -EIO;
   if(file)
      if((*file->operation->read)(file,
         data + (pos < 0 ? -pos : 0),0x1000 - ((pos < 0) ? -pos : 0),&base) < 0)
      goto failed; /*Read the data!*/
   if(!file)
      memset(data,0,0x1000); /*Set to zero!*/
   setPTEEntry(pte,address,va2pa(data));

   pagingFlushTLB();
   return 0;
failed:
   freePages(dataPage,0);
   return retval;
cow:;
   entry = getPTEEntry(pte,address);

   entryPage = getPhysicsPage(entry);
   if(atomicRead(&entryPage->count) == 1)
      goto done; /*Only one task is using it,we needn't copy it!*/
   dataPage = allocPages(0);
   if(!dataPage)
      return -ENOMEM;
   data = getPhysicsPageAddress(dataPage);
   memcpy((void *)data,(const void *)entry,0x1000);
             /*Copy the data.*/
   setPTEEntry(pte,address,va2pa(data));
   dereferencePage(entryPage,0); /*Now we don't use the page.*/
done:
   setPTEEntryAttribute(pte,address,1 /*Read/Write.*/);
   pagingFlushTLB();
   return 0;
}
