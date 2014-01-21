#include <core/const.h>
#include <cpu/io.h>
#include <video/console.h>
#include <interrupt/interrupt.h>
#include <interrupt/pic.h>
#include <interrupt/idt.h>
#include <interrupt/localapic.h>
#include <interrupt/ioapic.h>
#include <task/task.h>

typedef struct IRQInformation{
   IRQHandler handler;
   void *data;
} IRQInformation;

extern IRQHandler localApicTimerHandler;

static IRQInformation irqHandlerTable[IRQ_COUNT] = {}; 

int doIRQ(IRQRegisters *reg)
{
   int ret = 0;
   IRQInformation *info;
   switch(reg->irq)
   {
   case LOCAL_TIMER_INT: /*Local APIC Timer Interrupt?*/
      if(!localApicTimerHandler)
         break;
      startInterrupt();
      
      ret = (*localApicTimerHandler)(reg,0);

      closeInterrupt();
      break;
   default:
      if(reg->irq >= IRQ_COUNT)
         break;

      info = &irqHandlerTable[reg->irq];
      if(info->handler == 0)
         break;
      startInterrupt();

      ret = (*info->handler)(reg,info->data);

      closeInterrupt();
      break;
   }
   localApicSendEOI(); /*Send EOI to local APIC.*/
   if(ret == 1)/*Need schedule.*/
   {
      preemptionSchedule();
      closeInterrupt();
   }
   return ret;
}

int requestIRQ(u8 irq,IRQHandler handler)
{
   if(irq >= IRQ_COUNT)
      return -1;
   if(irqHandlerTable[irq].handler)
      return -1;
   irqHandlerTable[irq].handler = handler;
   irqHandlerTable[irq].data = 0;
   if(ioApicEnableIRQ(irq))
   {
      irqHandlerTable[irq].handler = 0;
      return -1; /*Can't enable this IRQ,restore irqHandlerTable.*/
   }
   return 0;
}

int setIRQData(u8 irq,void *data)
{
   if(irq >= IRQ_COUNT)
      return -1;
   if(!irqHandlerTable[irq].handler)
      return -1;
   irqHandlerTable[irq].data = data;
   return 0;
}

int freeIRQ(u8 irq)
{
   if(irq >= IRQ_COUNT)
      return -1;
   irqHandlerTable[irq].handler = 0;
   irqHandlerTable[irq].data = 0;
   return ioApicDisableIRQ(irq);
}

int initInterrupt(void)
{
   initIDT();
   initPIC(); /*In fact,it will disable PIC.*/

   if(initLocalApic())
      return -1;

   if(initIOApic())
      return -1;

   for(int i = 0;i < IRQ_COUNT;++i)
   {
      irqHandlerTable[i].handler = 0;
      irqHandlerTable[i].data = 0;
   }

   printkInColor(0x00,0xff,0x00,"Initialize interrupt successfully!\n\n");
   return 0;
}
