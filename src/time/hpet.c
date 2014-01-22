#include <core/const.h>
#include <acpi/acpi.h>
#include <time/hpet.h>
#include <interrupt/interrupt.h>
#include <lib/string.h>
#include <memory/paging.h>
#include <video/console.h>

/*See also 
 * http://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/software-developers-hpet-spec-1-0a.pdf
 * IA-PC HPET (High Precision Event Timers) Specification */

#define TIMER0_IRQ                0x2
/*If we don't use APIC,it should be 0x0.*/

#define FSEC_PER_SEC              1000000000000000ull

#define HPET_CONF_ENABLE          0x01
#define HPET_CONF_ENABLE_INT      0x02

#define HPET_TIMER_CONF_ENABLE    0x04
#define HPET_TIMER_CONF_PERIODIC  0x08
#define HPET_TIMER_CONF_PER_CAP   0x10
#define HPET_TIMER_CONF_SET_VAL   0x40

#define HPET_PERIOD_REG           0x04
#define HPET_CONF_REG             0x10
#define HPET_COUNTER_REG_LOW      0xf0
#define HPET_COUNTER_REG_HIGH     0xf4

#define HPET_TIMER_CONF_REG(t)    (0x100 + t*0x20)
#define HPET_TIMER_CMP_REG(t)     (0x108 + t*0x20)

static u8 *hpetAddress = 0;

static inline int hpetOut(u32 reg,u32 data)
   __attribute__ ((always_inline));
static inline u32 hpetIn(u32 reg)
   __attribute__ ((always_inline));

static inline int disableHpet(void)
   __attribute__ ((always_inline));

static inline int enableHpet(void)
   __attribute__ ((always_inline));

static inline int restartHpet(void)
   __attribute__ ((always_inline));

static inline u64 hpetReadCounter(void)
   __attribute__ ((always_inline));

static inline int enableHpetInterrupt(void)
   __attribute__ ((always_inline));

static inline int disableHpetInterrupt(void)
   __attribute__ ((always_inline));

static inline int hpetStartTimer(int index,int periodic,u32 time)
   __attribute__ ((always_inline));

static inline int hpetOut(u32 reg,u32 data)
{
   *(volatile u32 *)(hpetAddress + reg) = data;
   return 0;
}

static inline u32 hpetIn(u32 reg)
{
   return *(volatile u32 *)(hpetAddress + reg);
}

static inline u64 hpetReadCounter(void)
{
   u64 counter;
   disableHpet(); /*Must stop counting when we need to read the counter.*/
   counter = hpetIn(HPET_COUNTER_REG_LOW);
   counter |= ((u64)hpetIn(HPET_COUNTER_REG_HIGH)) << 32;
   enableHpet();
   return counter;
}

static inline int enableHpet(void)
{
   u32 conf = hpetIn(HPET_CONF_REG);
   conf |= HPET_CONF_ENABLE;
   hpetOut(HPET_CONF_REG,conf);
   return 0;
}

static inline int disableHpet(void)
{
   u32 conf = hpetIn(HPET_CONF_REG);
   conf &= ~HPET_CONF_ENABLE;
   hpetOut(HPET_CONF_REG,conf);
   return 0;
}

static inline int restartHpet(void)
{
   disableHpet();
   hpetOut(HPET_COUNTER_REG_LOW,0x0);
   hpetOut(HPET_COUNTER_REG_HIGH,0x0); /*Clear the counter.*/
   enableHpet();
   return 0;
}

static inline int enableHpetInterrupt(void)
{
   u32 conf = hpetIn(HPET_CONF_REG);
   conf |= HPET_CONF_ENABLE_INT;
   hpetOut(HPET_CONF_REG,conf);
   return 0;
}

static inline int disableHpetInterrupt(void)
{
   u32 conf = hpetIn(HPET_CONF_REG);
   conf &= ~HPET_CONF_ENABLE_INT;
   hpetOut(HPET_CONF_REG,conf);
   return 0;
}

static inline int hpetStartTimer(int index,int periodic,u32 time)
{
   u32 conf = hpetIn(HPET_TIMER_CONF_REG(index));
   if(periodic)
   {
      if(conf & HPET_TIMER_CONF_PER_CAP) /*Check support for periodic mode.*/
         conf |= HPET_TIMER_CONF_PERIODIC;
      else
         return -EPROTONOSUPPORT;
   }else{
      conf &= ~HPET_TIMER_CONF_PERIODIC;
   }
   conf |= (HPET_TIMER_CONF_ENABLE | HPET_TIMER_CONF_SET_VAL);
   hpetOut(HPET_TIMER_CONF_REG(index),conf);
   hpetOut(HPET_TIMER_CMP_REG(index),time); 
   /*We must first "hpetOut" conf,then "hpetOut" time.Or it won't work true.*/
   return 0;
}

int initHpet(IRQHandler handler,unsigned int hz)
{
   hpetAddress = getHpetAddress();
   if(!hpetAddress)
      return -ENODEV;
      /*Then we will use PIT,so we should not print this error.*/
   u32 period = hpetIn(HPET_PERIOD_REG);

   printk("HPET Period:%d,",period); 
   /*Freq is how many is added to the counter per second.*/

   unsigned long long hpetFreq = FSEC_PER_SEC;
   hpetFreq = hpetFreq / period;
   char temp[20];
   itoa(hpetFreq,temp,10,0,0,1);
   printk("Freq: %s\n",temp);

   hz = hpetFreq / hz;

   printk("Restart HPET and clear counter....\n");
   restartHpet();

   for(volatile int i = 0;i < 100000;++i)
      ; /*Wait a minute.*/
   
   u64 counter = hpetReadCounter();

   if(!counter)
   {
      printk("HPET didn't count,discard.\n");
      disableHpet();
      return -ENOSTR;
   }

   itoa(counter,temp,0x10,16,'0',1);
   printk("We waited a minute,now counter is 0x%s\n",temp);

   if(requestIRQ(TIMER0_IRQ,handler))
   {
      disableHpet();
      return -EBUSY;
   }
   if(hpetStartTimer(0,1,hz)) /*Start timer on IRQ2 (ACPI).*/
   {
      freeIRQ(TIMER0_IRQ);
      disableHpet();
      return -EPROTONOSUPPORT;
   }
   enableHpetInterrupt();

   return 0;
}
