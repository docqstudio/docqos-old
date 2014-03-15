#include <core/const.h>
#include <cpu/io.h>
#include <interrupt/interrupt.h>
#include <interrupt/localapic.h>
#include <time/localtime.h>
#include <time/time.h>
#include <video/console.h>
#include <task/task.h>

IRQHandler localApicTimerHandler = 0;

#define END_LOOPS                  20

static int localTimeInterrupt(IRQRegisters *reg,void *data)
{
   getCurrentTask()->needSchedule = 1;
   return 0;
}

int initLocalTime(void)
{
   unsigned long long start,now,tmp;
        /*Ticks.*/
   unsigned int lstart,lend;
        /*Local Apic Counter.*/
   printk("Calculate Local Apic Information.");

   localApicTimerHandler = 0; /*Set the handler to zero.*/
   setupLocalApicTimer(0,0xffffffff);
        /*Start the local apic timer.*/
   start = getTicks();
      /*Get the ticks.*/

   startInterrupt();
   while((now = getTicks()) == start)
      asm("hlt"); /*Wait until the ticks change.*/
   lstart = getLocalApicTimerCounter();
           /*Now get the counter of local apic timer.*/
   start = tmp = now;
           /*Init start tmp now.*/

   while((now = getTicks()) - start < TIMER_HZ * 3)
      if(now - tmp > TIMER_HZ / 10 && (tmp = now))
         printk("."); /*Print '.' to the screen.*/
   lend = getLocalApicTimerCounter();
        /*OK,get the counter again.*/

   closeInterrupt(); 
   setupLocalApicTimer(1,0); 
          /*Disable local apic timer.*/

   printk("\n");
   u32 freq = lstart - lend;
       /*Note:The counter is reducing!!!*/
   freq /= (TIMER_HZ * 3);
   if(!freq)
   {
      printkInColor(0xff,0x00,0x00,"Local APIC Timer count too slow!!!");
      return -EOVERFLOW;
   }
   printk("Counter Freq: %d\n",freq);

   localApicTimerHandler = localTimeInterrupt; /*Set the handler.*/
   setupLocalApicTimer(0x0,freq);/*Enable local apic timer.*/
   
   printkInColor(0x00,0xff,0x00,"Initialize Local Apic Timer successfully!\n");

   return 0;
}
