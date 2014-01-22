#include <core/const.h>
#include <interrupt/localapic.h>
#include <interrupt/idt.h>
#include <cpu/cpuid.h>
#include <video/console.h>
#include <acpi/acpi.h>

#define LOCAL_APIC_ID            0x020 /*Local Apic ID.*/
#define LOCAL_APIC_TPR           0x080 /*Task Priority.*/
#define LOCAL_APIC_EOI           0x0b0 /*Send EOI.*/
#define LOCAL_APIC_DFR           0x0e0 /*Destination Format*/
#define LOCAL_APIC_LDR           0x0d0 /*Logical Destination.*/
#define LOCAL_APIC_SVR           0x0f0 /*Spurious Interrupt Vector.*/
#define LOCAL_APIC_LVT           0x320 /*Local Vector Table(LVT). (Timer.)*/
#define LOCAL_APIC_TICR          0x380 /*Initial Count For Timer.*/
#define LOCAL_APIC_TCCR          0x390 /*Current Count For Timer.*/
#define LOCAL_APIC_TDCR          0x3e0 /*Device Configuration For Timer.*/

static u8 *localApicAddress = 0;

static inline u32 localApicIn(u32 reg) __attribute__ ((always_inline));
static inline int localApicOut(u32 reg,u32 data) __attribute ((always_inline));

static inline u32 localApicIn(u32 reg)
{
   return *(volatile u32 *)(localApicAddress + reg);
}

static inline int localApicOut(u32 reg,u32 data)
{
   *(volatile u32 *)(localApicAddress + reg) = data;
   return 0;
}

int setupLocalApicTimer(int disable,u32 time)
/*Disable Local Apic Timer if interruptVector == 0.*/
{
   if(!time)
      time = localApicIn(LOCAL_APIC_TICR);
      /*There will have some errors if we only write LOCAL_APIC_IVT.*/
      /*Such as virtualbox.*/
   localApicOut(LOCAL_APIC_TDCR,0x3);
   localApicOut(LOCAL_APIC_TICR,time);
   localApicOut(LOCAL_APIC_LVT,
      disable ? 0x0 : (LOCAL_TIMER_INT | 0x20000));
   return 0;
}

int initLocalApic(void)
{
   if(!checkIfCPUHasApic())
   {
      printkInColor(0xff,0x00,0x00,"Your CPU doesn't have local apic!!!\n");
      return -ENODEV;
   }
   if((localApicAddress = getLocalApicAddress()) == 0)
   {
      printkInColor(0xff,0x00,0x00,"Can't get local apic address!!!\n");
      return -ENODEV;
   }

   localApicOut(LOCAL_APIC_TPR,0x0);

   localApicOut(LOCAL_APIC_DFR,0xffffffff);
   localApicOut(LOCAL_APIC_LDR,
      ((u32)(pointer)(localApicAddress + LOCAL_APIC_LDR) & 0x00FFFFFF) | 1);

   localApicOut(LOCAL_APIC_SVR,0x100 | 0xff); /*Init Local Apic.*/

   setupLocalApicTimer(1,0); /*Disable.*/

   printk("Initialize Local Apic successfully!\n");
   return 0;
}

u8 getLocalApicID(void)
{
   return (u8)(localApicIn(LOCAL_APIC_ID) >> 24);
}

int localApicSendEOI(void)
{
   return localApicOut(LOCAL_APIC_EOI,0x0);
}

