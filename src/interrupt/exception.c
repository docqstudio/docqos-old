#include <core/const.h>
#include <lib/string.h>
#include <video/console.h>

typedef struct ExceptionRegisters
{
   u64 rdi,rsi,rbp,rbx,rdx,rcx,rax; /*Pushed after jmp exception.*/
   u64 exceptionNumber; /*Pushed before jmp exception.*/
   u64 errorCode; /*Maybe CPU pushed.*/
   u64 rip,cs,rflags,rsp,ss; /*CPU pushed.*/
} __attribute__ ((packed)) ExceptionRegisters;

int handleException(ExceptionRegisters regs) __attribute__((regparm(3)));
int handleException(ExceptionRegisters regs)
{
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
   char buf[32];
   buf[0] = '0';
   buf[1] = 'x';
   printkInColor(0xff,0x00,0x00,"\n\nException!!!!!!! Type:%s\nRegisters:\n",
      exceptionInformation[regs.exceptionNumber]);

#define _i(reg)                                       \
do{                                                   \
   itoa(regs.reg,buf + 2,0x10,16,'0',1);              \
   printkInColor(0xff,0x00,0x00,#reg "=> %s ",buf);   \
}while(0)

   _i(rax);
   _i(rbx);
   _i(rcx);
   _i(rdx);
   printk("\n");
   _i(rbp);
   _i(rsi);
   _i(rdi);
   _i(rsp);
   printk("\n");
   _i(rflags);
   _i(rip);
   printk("\n");

#undef _i

#define _i(reg) \
do{                                                \
itoa(regs.reg,buf + 2,0x10,4,'0',1);               \
printkInColor(0xff,0x00,0x00, #reg "=> %s ",buf);  \
}while(0)

   _i(cs);
   _i(ss);
   printk("\n");

#undef _i

   if(regs.errorCode != (u64)-1)
   {
      itoa(regs.errorCode,buf + 2,0x10,4,'0',1);
      printkInColor(0xff,0x00,0x00,"Error Code=> %s\n",buf);
   }
   for(;;);
   return 0;
}
