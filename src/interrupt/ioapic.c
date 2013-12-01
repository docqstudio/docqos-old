#include <core/const.h>
#include <interrupt/ioapic.h>
#include <interrupt/localapic.h>
#include <interrupt/idt.h>
#include <acpi/acpi.h>
#include <video/console.h>

#define IOREGSEL            0x00
#define IOWIN               0x10

#define IOAPICVER           0x01
#define IOREDTBL            0x10

static u8 *ioApicAddress = 0;

static u8 irqCount = 0;

static inline u32 ioApicIn(u8 reg) __attribute__ ((always_inline));
static inline int ioApicOut(u8 reg,u32 data) __attribute__ ((always_inline)) ;

static inline u32 ioApicIn(u8 reg)
{
   *(volatile u32 *)(ioApicAddress + IOREGSEL) = reg;
   return *(volatile u32 *)(ioApicAddress + IOWIN);
}

static inline int ioApicOut(u8 reg,u32 data)
{
   *(volatile u32 *)(ioApicAddress + IOREGSEL) = reg;
   *(volatile u32 *)(ioApicAddress + IOWIN) = data;
   return 0;
}

static int ioApicSetIRQData(u8 irq,u64 data)
{
   ioApicOut(IOREDTBL + irq * 2,(u32)data);
   ioApicOut(IOREDTBL + irq * 2 + 1,(u32)(data >> 32));
   return 0;
}

static int ioApicSetIRQ(u8 irq,u8 interruptVector,u8 localApicID,u8 disable)
{
   if(interruptVector)
   {
      u64 data = (u64)interruptVector | ((u64)localApicID) << 56;
      if(disable)
         data |= 1 << 16;
      ioApicSetIRQData(irq,data);
   }else
   {
      ioApicSetIRQData(irq,1 << 16);
   }
   return 0;
}

int ioApicEnableIRQ(u8 irq)
{
   if(irq >= irqCount)
      return -1;
   u32 data = ioApicIn(IOREDTBL + irq * 2); 
   data &= ~(1 << 16);
   ioApicOut(IOREDTBL + irq * 2,data);
   return 0;
}

int ioApicDisableIRQ(u8 irq)
{
   if(irq >= irqCount)
      return -1;
   u32 data = ioApicIn(IOREDTBL + irq * 2); 
   data |= (1 << 16);
   ioApicOut(IOREDTBL + irq * 2,data);
   return 0;
}

int initIOApic(void)
{
   if((ioApicAddress = getIOApicAddress()) == 0)
   {
      printkInColor(0xff,0x00,0x00,"Can't get I/O Apic Address!!!.");
      return -1;
   }
   u32 x = ioApicIn(IOAPICVER);
   int count = ((x >> 16) & 0xff) + 1;
   u8 localApicID = getLocalApicID();
   printk("I/O Apic IRQ counts = %d.\n",count);

   count = (IRQ_COUNT > count)?count:IRQ_COUNT; /*Min!*/
   irqCount = count;
   for(int i = 0;i < count;++i)
   {
      ioApicSetIRQ(i,i + IRQ_START_INT,localApicID,1 /*Disable.*/);
   }

   printk("Initialize I/O Apic successfully.\n");
   return 0;
}
