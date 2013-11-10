#include <core/const.h>
#include <interrupt/idt.h>
#include <video/console.h>

typedef struct LongModeGate{
   u16 offset1;
   u16 selector;

   u16 type; /*Call-Gate doesn't have IST,but Interrupt-Gate and Trap-Gate has.*/
            /*It's the only difference betweens three kinds of gates (in long mode).*/
   u16 offset2;

   u32 offset3;

   u32 reserved;
} __attribute__ ((packed)) LongModeGate;

#define IDT_MAX_SIZE        0xff
#define INTERRUPT_GATE_TYPE 0x8e00 /*IST = 0 .*/
#define TRAP_GATE_TYPE      0x8f00 /*IST = 0 .*/

static u8 idt[256 * sizeof(LongModeGate)];
static u8 idtr[2 + 8];

extern void (*exceptionHandlers[20])(void);
/*Defined in exception.S .*/
extern void defaultInterruptHandler(void);
/*Defined in interrupt.S .*/

int setIDTGate(u8 index,u16 type, void (*handler)());

int initIDT(void)
{
   for(int i = 0x0;i < 20;++i)
   {
      setIDTGate(i,INTERRUPT_GATE_TYPE,exceptionHandlers[i - 0x0]);
   }
   for(int i = 20;i < 256;++i)
   {
      setIDTGate(i,INTERRUPT_GATE_TYPE,defaultInterruptHandler);
   }
/*All gates in IDT are interrupt gates.*/

   *(u16 *)idtr = (u16)(sizeof(idt) - 1);
   *(u64 *)(idtr + 2) = (u64)idt; /*Init idtr.*/
   asm("lidt (%%rax)"::"a"(idtr)); /*Load idt.*/

   printk("Initialize IDT successfully!\n");

   return 0;
}

int setLongModeGate(LongModeGate *gate,u16 type, u64 base)
{
   u16 cs = 0;

   asm volatile("movw %%cs,%%ax"
      :"=a"(cs)); /*Saved %cs to cs.*/

   gate->offset1 = (u16)(base);
   gate->offset2 = (u16)(base >> 16);
   gate->offset3 = (u32)(base >> 32);
   gate->reserved = 0;
   gate->type = type;
   gate->selector = cs;
   return 0;
}

int setIDTGate(u8 index,u16 type,void (*handler)())
{
   LongModeGate *gate = (LongModeGate *)(idt + sizeof(LongModeGate)*index);
   if(handler)
      setLongModeGate(gate,type,(u64)(handler));
   else
      setLongModeGate(gate,0,0);
   return 0;
}
