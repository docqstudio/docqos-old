#include <core/const.h>
#include <time/time.h>
#include <time/hpet.h>
#include <time/pit.h>
#include <interrupt/interrupt.h>
#include <video/console.h>
#include <task/task.h>
#include <cpu/io.h>
#include <cpu/spinlock.h>
#include <memory/kmalloc.h>

static ListHead timers;
static SpinLock timerLock;
static unsigned long long ticks = 0;

static int timeInterrupt(IRQRegisters *reg)
{
   u64 rflags;
   ++ticks;

   for(;;){
      lockSpinLockDisableInterrupt(&timerLock,&rflags);

      if(listEmpty(&timers))
         break; /*Just break if empty.*/
      Timer *timer = listEntry(timers.next,Timer,list);
      if(timer->ticks > ticks)
         break; /*Break if timers are not timeout.*/
      listDelete(&timer->list); /*Timeout!Delete it.*/
      
      unlockSpinLockRestoreInterrupt(&timerLock,&rflags);
      
      (*timer->callback)(timer->data); /*Call callback.*/
      if(!timer->onStack)
         kfree(timer); /*If it created by createTimer,kfree it.*/
   }

   unlockSpinLockRestoreInterrupt(&timerLock,&rflags);
      /*Need unlock.*/

   return 0;
}

int initTime(void)
{
   if(initHpet(timeInterrupt,TIMER_HZ))
      if(initPit(timeInterrupt,TIMER_HZ))
         return -1;

   initList(&timers);
   initSpinLock(&timerLock);
   printkInColor(0x00,0xff,0x00,"Initialize time successfully!\n\n");
   return 0;
}

unsigned long long getTicks(void)
{
   return ticks;
}

int initTimer(Timer *timer,
   TimerCallBackFunction callback,int timeout,void *data)
{ /*This val `data` is an argument when we call `callback`.*/
   timer->callback = callback;
   timer->ticks = ticks + timeout; 
   timer->onStack = 1; 
      /*This field decides if we call kfree when this timer is timeout.*/
   timer->data = data;
   initList(&timer->list);
   return 0;
}

Timer *createTimer(TimerCallBackFunction callback,int timeout,void *data)
{
   Timer *timer = (Timer *)kmalloc(sizeof(Timer));
   initTimer(timer,callback,timeout,data);
   timer->onStack = 0; /*It will be free auto.*/
   return timer;
}

int addTimer(Timer *timer)
{
   u64 rflags;
   lockSpinLockDisableInterrupt(&timerLock,&rflags);

   for(ListHead *list = timers.next;list != &timers;list = list->next)
   {
      Timer *__timer = listEntry(list,Timer,list);
      if(__timer->ticks > timer->ticks)
      { /*Add this timer to a appropriate position.*/
         listAddTail(&timer->list,&__timer->list);
         goto end;
      }
   }
   /*Add this timer to the last position.*/
   listAddTail(&timer->list,&timers);
end:
   unlockSpinLockRestoreInterrupt(&timerLock,&rflags);
   return 0;
}
