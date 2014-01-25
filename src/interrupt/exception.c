#include <core/const.h>
#include <lib/string.h>
#include <video/console.h>
#include <interrupt/interrupt.h>
#include <cpu/io.h>

typedef int (*ExceptionHandler)(IRQRegisters *reg);

extern int doPageFault(IRQRegisters *reg);

static ExceptionHandler doExceptions[20] =
{
   [0 ... 13] = 0,
   [14] = doPageFault,
   [15 ... 19] = 0
};

static const char *exceptionInformation[20] =
{
    [0] = "Divide Error",
    [1] = "Debug",
    [2] = "Nonmaskable Interrupt",
    [3] = "Breakpoint",
    [4] = "Overflow",
    [5] = "Bound Range Exceeded",
    [6] = "Invalid Opcode",
    [7] = "Device Not Available",
    [8] = "Double Fault",
    [9] = "Coprocessor Segment Overrun",
    [10] = "Invalid TSS",
    [11] = "Segment Not Present",
    [12] = "Stack-Segment Fault",
    [13] = "General Protection",
    [14] = "Page Fault",
    [16] = "Floating Point Error",
    [17] = "Alignment Check",
    [18] = "Machine Check",
    [19] = "SIMD Exception"
};

int handleException(IRQRegisters *regs)
{
   u8 vector = (regs->irq >> 56) & 0xff;
   const char *type;
   char buf[32];
   buf[0] = '0';
   buf[1] = 'x';

   regs->irq &= 0x00fffffffffffffful;
   if(vector < sizeof(doExceptions) / sizeof(doExceptions[0]))
      if(doExceptions[vector])
         if(!(*doExceptions[vector])(regs))
            return 0;

   closeInterrupt();
   if(vector >= sizeof(exceptionInformation) / sizeof(exceptionInformation[0]))
      type = "Unknow Type";
   else
      type = exceptionInformation[vector];
   printkInColor(0xff,0x00,0x00,"\n\nException!!!!!!! Type:%s\nRegisters:\n",
      type);

#define printr(reg)                                       \
   do{                                                    \
      itoa(regs->reg,buf + 2,0x10,16,'0',1);               \
      printkInColor(0xff,0x00,0x00,#reg "=> %s ",buf);    \
   }while(0)

   printr(rax);
   printr(rbx);
   printr(rcx);
   printr(rdx);
   printk("\n");
   printr(rbp);
   printr(rsi);
   printr(rdi);
   printr(rsp);
   printk("\n");
   printr(rflags);
   printr(rip);
   printk("\n");

#undef printr

#define printr(reg)                                      \
   do{                                                   \
      itoa(regs->reg,buf + 2,0x10,4,'0',1);               \
      printkInColor(0xff,0x00,0x00, #reg "=> %s ",buf);  \
   }while(0)

   printr(cs);
   printr(ss);
   printk("\n");

#undef printr

   if(regs->irq)
   {
      itoa(regs->irq,buf + 2,0x10,4,'0',1);
      printkInColor(0xff,0x00,0x00,"Error Code=> %s\n",buf);
   }
   for(;;);
   return 0;
}
