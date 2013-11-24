#pragma once

typedef struct IRQRegisters{
   u64 r15,r14,r13,r12,r11,r10,r9,r8; /*\ */
   u64 rdi,rsi,rbp,rdx,rcx,rbx,rax;   /*| We pushed them.*/
   u64 irq;                           /*/ */

   u64 rip,cs,rflags,rsp,ss; /*CPU pushed them.*/
} __attribute__ ((packed)) IRQRegisters;

typedef int (*IRQHandler)(IRQRegisters *reg);

int initInterrupt(void);

int requestIRQ(u8 irq,IRQHandler handler);
int freeIRQ(u8 irq);
