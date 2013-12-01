#include <core/const.h>
#include <cpu/io.h>
#include <interrupt/interrupt.h>
#include <interrupt/localapic.h>
#include <time/localtime.h>
#include <time/time.h>
#include <video/console.h>

IRQHandler localApicTimerHandler = 0;

#define END_LOOPS                  20

static volatile int loops = 0;                   /* \ */
static unsigned long long startTicks = 0;        /* | */
static unsigned long long endTicks   = 0;        /* | Only use in kernel init.*/
                                                 /* | */
static int calcFreqInterrupt(IRQRegisters *reg)   /* / */
{
   switch(loops)
   {
   case 0:
      startTicks = getTicks();
      break;
   case END_LOOPS - 1:
      endTicks = getTicks();
      break;
   }
   ++loops;
   printk(".");
   return 0;
}

static int localTimeInterrupt(IRQRegisters *reg)
{
   return 1;/*Schedule.*/
}

int initLocalTime(void)
{
   printk("Calculate Local Apic Information.");

   localApicTimerHandler = calcFreqInterrupt;
   loops = 0;

   setupLocalApicTimer(0,0xffffff);
   startInterrupt();
   while(loops != END_LOOPS)
      asm("hlt"); 
      /*Wait for local APIC timer interrupt END_LOOPS times.*/
   closeInterrupt();
   setupLocalApicTimer(1,0); /*Disable.*/

   printk("\n");
   u32 ticks = endTicks - startTicks;
   ticks /= END_LOOPS;
   if(!ticks)
   {
      printkInColor(0xff,0x00,0x00,"Local APIC Timer count too quick!!!\n");
      return -1;
   }
   ticks = 0xffffff / ticks;
   if(!ticks)
   {
      printkInColor(0xff,0x00,0x00,"Local APIC Timer count too slow!!!!\n");
      return -1;
   }
   printk("Ticks: %d\n",ticks);

   localApicTimerHandler = localTimeInterrupt;
   setupLocalApicTimer(0x0,ticks);/*Enable.*/
   
   printkInColor(0x00,0xff,0x00,"Initialize Local Apic Timer successfully!\n");

   return 0;
}
