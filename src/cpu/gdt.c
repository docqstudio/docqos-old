#include <core/const.h>
#include <cpu/gdt.h>
#include <video/console.h>

typedef struct GDTDescriptor /*Code or data.*/
{
   u16 limit1;
   u16 address1;

   u8 address2;
   u8 type1;

   u8 limit2:4;
   u8 type2:4;
   u8 address3;
} __attribute__ ((packed)) GDTDescriptor;

typedef struct GDTSystemDescriptor
{
   u16 limit1;
   u16 address1;

   u8 address2;
   u8 type1;

   u8 limit2:4;
   u8 type2:4;
   u8 address3;

   u32 address4;
   u32 reserved;
} __attribute__ ((packed)) GDTSystemDescriptor;

typedef struct TaskStateSegment{
   u32 reserved1;
   u64 rsp0;
   u64 rsp1;
   u64 rsp2;

   u64 reserved2;

   u64 ist1;
   u64 ist2;
   u64 ist3;
   u64 ist4;
   u64 ist5;
   u64 ist6;
   u64 ist7;

   u64 reserved3;
   u16 reserved4;

   u16 ioMapAddress;
} __attribute__ ((packed)) TaskStateSegment;

/*See also https://sparrow.ece.cmu.edu/group/731-s07/readings/amd2_24593.pdf */
/*Note: sizeof(GDTSystemDescriptor) = 64 ,but sizeof(GDTDescriptor) = 32.*/

#define GDT_SIZE ((10 + 1) * sizeof(GDTDescriptor))

#define DA_PRESENT  0x0080

#define DA_CODE     0x0018
#define DA_CODE_64  0x2000

#define DA_DATA     0x0010
#define DA_DATA_W   0x0002

#define DA_TSS      0x0009

#define DA_DPL3     0x0080
#define DA_DPL2     0x0060
#define DA_DPL1     0x0040
#define DA_DPL0     0x0000

static u8 gdt[GDT_SIZE] = {};
static u8 gdtr[2 + 8] = {};

static TaskStateSegment taskStateSegment = {};

static int setCodeDataDescriptor(u32 index,u16 type)
{
   GDTDescriptor *desc = (GDTDescriptor *)(gdt + index);
   desc->type1 = type & 0x00ff;
   desc->type2 = (type & 0xf000) >> 12;
   return (int)((u8 *)(desc + 1) - gdt);
}

static int setSystemDescriptor(u8 index,u16 type,pointer address,u32 limit)
{
   GDTSystemDescriptor *desc = (GDTSystemDescriptor *)(gdt + index);
   desc->type1 = type & 0x00ff;
   desc->type2 = (type & 0xf000) >> 12;

   desc->address1 = address & 0xffff;
   desc->address2 = (address & 0xff0000) >> 16;
   desc->address3 = (address & 0xff000000) >> 24;
   desc->address4 = address >> 32;

   desc->limit1 = limit & 0xffff;
   desc->limit2 = (limit & 0xf0000) >> 16;

   desc->reserved = 0;
   return (int)((u8 *)(desc + 1) - gdt);
}

int tssSetStack(u64 stack)
{
   taskStateSegment.rsp0 = stack;
   return 0;
}

int initGDT(void)
{
   int index = sizeof(GDTDescriptor);
   index = setCodeDataDescriptor(index,
      DA_CODE | DA_CODE_64 | DA_PRESENT);
   index = setCodeDataDescriptor(index,
      DA_DATA | DA_DATA_W  | DA_PRESENT | DA_DPL3);

   index = setSystemDescriptor(index,
      DA_TSS | DA_PRESENT,
      (pointer)&taskStateSegment,sizeof(TaskStateSegment));

   *(u16 *)gdtr = (GDT_SIZE - 1);
   *(u64 *)(gdtr + 2) = (u64)(pointer)gdt;

   asm volatile(
      "lgdt (%%rax)"
      :
      : "a"(gdtr)
   ); /*Load new GDT.*/

   asm volatile(
      "movw %%ax,%%ds\n\t"
      "movw %%ax,%%gs\n\t"
      "movw %%ax,%%es\n\t"
      "movw %%ax,%%ss\n\t"
      "movw %%ax,%%fs\n\t"
      "movq %%rsp,%%rcx\n\t"
      "movabs $1f,%%rdx\n\t"
      "pushq %%rax\n\t"
      "pushq %%rcx\n\t"
      "pushfq\n\t"
      "pushq %%rbx\n\t"
      "pushq %%rdx\n\t"
      "iretq\n\t"
      "1:"
      :
      : "a" ((u64)SELECTOR_DATA),"b" ((u64)SELECTOR_KERNEL_CODE)
      : "%rdx","%rcx"
   ); /*Refresh segment registers.*/

   asm volatile(
      "ltr %%ax"
      :
      : "a" ((u64)SELECTOR_TSS)
   ); /*Load TSS.*/

   printkInColor(0x00,0xff,0x00,"Initialize GDT successfully!!");

   return 0;
}
