#include <core/const.h>
#include <cpu/io.h>
#include <video/console.h>
#include <interrupt/interrupt.h>
#include <interrupt/pic.h>
#include <interrupt/idt.h>
#include <interrupt/localapic.h>
#include <interrupt/ioapic.h>
#include <task/task.h>

extern IRQHandler localApicTimerHandler;

static IRQHandler irqHandlerTable[IRQ_COUNT] = {
   [0 ... IRQ_COUNT - 1] = 0
}; 

int doIRQ(IRQRegisters reg)
{
   localApicSendEOI(); /*Send EOI to local APIC.*/
   int ret;

   switch(reg.irq)
   {
   case LOCAL_TIMER_INT: /*Local APIC Timer Interrupt?*/
      setupLocalApicTimer(1,0); /*Disable.*/
      if(!localApicTimerHandler)
         return 0;
      startInterrupt();
      
      ret = localApicTimerHandler(&reg);

      closeInterrupt();
      setupLocalApicTimer(0,0); /*Enable.*/
      break;
   default:
      ioApicDisableIRQ((u8)reg.irq);

      if(irqHandlerTable[reg.irq] == 0)
         return 0; /*We needn't to enable this IRQ,it should be disabled.*/
      startInterrupt();

      ret = (irqHandlerTable[reg.irq])(&reg);
   
      closeInterrupt();
      ioApicEnableIRQ((u8)reg.irq); /*Enable this irq.*/
      break;
   }
   if(ret == 1)/*Need schedule.*/
   {
      schedule();
      closeInterrupt();
   }
   return ret;
}

int requestIRQ(u8 irq,IRQHandler handler)
{
   if(irq > IRQ_COUNT)
      return -1;
   if(irqHandlerTable[irq])
      return -1;
   irqHandlerTable[irq] = handler;
   if(ioApicEnableIRQ(irq))
   {
      irqHandlerTable[irq] = 0;
      return -1; /*Can't enable this IRQ,restore irqHandlerTable.*/
   }
   return 0;
}

int freeIRQ(u8 irq)
{
   if(irq > IRQ_COUNT)
      return -1;
   irqHandlerTable[irq] = 0;
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

   printkInColor(0x00,0xff,0x00,"Initialize interrupt successfully!\n\n");
   return 0;
}
