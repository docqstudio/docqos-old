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

   printkInColor(0xff,0x00,0x00,
      "rax=> 0x%016lx,rbx=> 0x%016lx,rcx=> 0x%016lx,rdx=> 0x%016lx\n",
      regs->rax,regs->rbx,regs->rcx,regs->rdx);
   printkInColor(0xff,0x00,0x00,
      "rbp=> 0x%016lx,rsi=> 0x%016lx,rdi=> 0x%016lx,rsp=> 0x%016lx\n",
      regs->rbp,regs->rsi,regs->rdi,regs->rsp);
   printkInColor(0xff,0x00,0x00,
      "rflags=> 0x%016lx,rip=> 0x%016lx\n",
      regs->rflags,regs->rip);

   printkInColor(0xff,0x00,0x00,
      "cs=> 0x%04x,ss=> 0x%04x\n",
      regs->cs,regs->ss);

   if(regs->irq)
      printkInColor(0xff,0x00,0x00,
         "Error Code=> 0x%04x",
         regs->irq);
   for(;;);
   return 0;
}
