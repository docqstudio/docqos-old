#include <core/const.h>
#include <memory/paging.h>
#include <memory/memory.h>
#include <memory/buddy.h>
#include <lib/string.h>
#include <filesystem/virtual.h>

#define MIN_MAPPING (1024ul*1024*1024*4) /*4GB.*/

extern void *endAddressOfKernel;

static u64 *kernelPML4EDir;
static u64 *kernelPDPTEDir;
static u64 *kernelPDEDir;

static u64 pml4eCount;

int calcMemorySize(void);

int initPaging(void)
{
   calcMemorySize();

   u64 mappingSize = getMemorySize();
   if(mappingSize < MIN_MAPPING)
      mappingSize = MIN_MAPPING;
   u64 pdeCount = mappingSize / (1024*1024*2); /*2MB.*/
   u64 pdpteCount = pdeCount / 512;
   pml4eCount = pdpteCount / 512;
   u64 *endAddress;

   if(!pdpteCount)
      pdpteCount = 1;
   if(!pml4eCount)
      pml4eCount = 1;
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
      kernelPDEDir[i] = ((u64)i) * (1024*1024*2) + 0x183;
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

void *allocPML4E(void)
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

void *allocPDPTE(void *__pml4e,pointer address)
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

void *allocPDE(void *__pdpte,pointer address)
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

void *allocPTE(void *__pde,pointer address)
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

int setPTE(void *__pte,pointer address,pointer p)
{
   u64 *pte = (u64 *)__pte;
   u64 nr = (address >> 12) & 0x1ff;
   u64 data = pte[nr];
   if(data & 0x1) /*It has set?*/
      return -1;
   pte[nr] = ((u64)p) + 0x007; /*P,R/W and U/S.*/
   return 0;
}

int doMMap(VFSFile *file,u64 offset,pointer address,u64 len)
{
   Task *current = getCurrentTask();
   void *pml4e;
   PhysicsPage *page = 0;
   if(current->cr3)
      pml4e = pa2va(current->cr3); /*Ignore PWD and PCD.*/
   else
   {
      pml4e = allocPML4E();
      if(!pml4e)
         return -1;
      current->cr3 = va2pa(pml4e);
   }
   len = (len + 0xfff) & ~0xfff;
   address &= ~0xfff;
   if(address + len < address || address + len > PAGE_OFFSET)
      return -1; /*Can't map kernel pages.*/
   len >>= 12; /*Get pages which need map.*/
   if(file)
      if(lseekFile(file,offset) < 0)
         return -1;
   while(len--)
   {
      page = allocPages(0);
      if(!page)
         goto failed;
      void *data = getPhysicsPageAddress(page);
      if(file)
         if(readFile(file,data,0x1000) <= 0)
            goto failed;
      /*If file is null,map a page without data.*/
      void *pdpte = allocPDPTE(pml4e,address);
      if(!pdpte)
         goto failed;
      void *pde = allocPDE(pdpte,address);
      if(!pde)
         goto failed;
      void *pte = allocPTE(pde,address);
      if(!pte)
         goto failed;
      if(setPTE(pte,address,va2pa(data)))
         goto failed;
      address += 0x1000; /*Next page.*/
   }
   asm volatile("movq %%rax,%%cr3"::"a"(current->cr3));
      /*Refresh TLBs.*/
   return 0;
failed:
   if(page)
      freePages(page,0);
   return -1;
}
