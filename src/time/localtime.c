#include <core/const.h>
#include <cpu/io.h>
#include <interrupt/interrupt.h>
#include <interrupt/localapic.h>
#include <time/localtime.h>
#include <time/time.h>
#include <video/console.h>

int (*localApicTimerHandler)(IRQRegisters reg) = 0;

static volatile int happen = 1;                  /*\ */
                                                 /*| Only use in kernel init.*/
static int calcFreqInterrupt(IRQRegisters reg)   /*/ */
{
   happen = 0;
   return 0;
}

static int localTimeInterrupt(IRQRegisters reg)
{
   return 0;
}

int initLocalTime(void)
{
   happen = 1;
   localApicTimerHandler = calcFreqInterrupt;
   unsigned int temp;
   volatile unsigned long long ticks = getTicks();
   setupLocalApicTimer(0,0xffffff);

   startInterrupt();
   while(happen);
   closeInterrupt();

   ticks = (getTicks() - ticks);
   setupLocalApicTimer(1,0x100);
   if(!ticks)
   {
      printkInColor(0xff,0x00,0x00,"Init Local Apic Timer Failed!\n");
      return -1;
   }

   ticks *= MSEC_PER_SEC / TIMER_HZ;
   ticks = 0xffffff / ticks;
   printk("Setup Local Apic Timer:Time => %d\n",ticks);
   if(!ticks)
   {
      printkInColor(0xff,0x00,0x00,"Local Apic Timer count too slow!\n");
      return -1;
   }

   printk("Test Local Apic Timer again...\n");

   happen = 1;
   temp = ticks;
   ticks = getTicks();
   setupLocalApicTimer(0,temp);

   startInterrupt();
   while(happen);
   closeInterrupt();


   ticks = (getTicks() - ticks);

   if(ticks > 3)
   {
      printkInColor(0xff,0x00,0x00,"Local Apic Timer count too quick!");
      return -1;
   }
   localApicTimerHandler = localTimeInterrupt;
   setupLocalApicTimer(0,temp); /*Enable and return.*/

   printkInColor(0x00,0xff,0x00,"Initialize Local Apic Timer successfully!");
   return 0;
}
