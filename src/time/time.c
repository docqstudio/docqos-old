#include <core/const.h>
#include <time/time.h>
#include <time/hpet.h>
#include <time/pit.h>
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
      if(initPit(timeInterrupt,TIMER_HZ))
         return -1;

   printkInColor(0x00,0xff,0x00,"Initialize time successfully!\n\n");
   return 0;
}

unsigned long long getTicks(void)
{
   return ticks;
}
