#include <core/const.h>
#include <time/time.h>
#include <time/hpet.h>
#include <interrupt/interrupt.h>
#include <video/console.h>

static unsigned long long ticks = 0;

static int timeInterrupt(IRQRegisters *reg)
{
   ++ticks;
   return 0;
}

int initTime(void)
{
   if(initHpet(timeInterrupt,TIMER_HZ))
   {
      printkInColor(0xff,0x00,0x00,"No support for HPET,discard.");
      /*We will support PIT next version.*/
      return -1;
   }
   printkInColor(0x00,0xff,0x00,"Initialize time successfully!\n\n");
   return 0;
}

unsigned long long getTicks(void)
{
   return ticks;
}
