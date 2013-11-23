#include <core/const.h>
#include <memory/paging.h>
#include <memory/memory.h>

#define MIN_MAPPING (1024ul*1024*1024*4) /*4GB.*/

extern void *endAddressOfKernel;

static u64 *kernelPML4EDir;
static u64 *kernelPDPTEDir;
static u64 *kernelPDEDir;

int calcMemorySize(void);

int initPaging(void)
{
   calcMemorySize();

   u64 mappingSize = getMemorySize();
   if(mappingSize < MIN_MAPPING)
      mappingSize = MIN_MAPPING;
   u64 pdeCount = mappingSize / (1024*1024*2); /*2MB.*/
   u64 pdpteCount = pdeCount / 512;
   u64 pml4eCount = pdpteCount / 512;
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
      kernelPDEDir[i] = ((u64)i) * (1024*1024*2) + 0x83;
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
      kernelPDPTEDir[i] = ((u64)va2pa(&kernelPDEDir[i * 512])) + 0x03;
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
      kernelPML4EDir[i] = ((u64)va2pa(&kernelPDPTEDir[(i - 1) * 512])) + 0x03;
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
