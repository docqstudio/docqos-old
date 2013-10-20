.set inLoader,0

.global _start

.section ".text" , "ax"
.code16

_start: .ascii "loader"
start:
   jmp entry32

entry32: .code16
   movw %cs,%ax
   movw %ax,%ds
   movw %ax,%gs
   movw %ax,%es /*Init segment registers again.*/

   lgdt (GDTR) /*Load GDT.*/

   cli /*Close int.*/

   inb $0x92,%al
   orb $0x2,%al
   outb %al,$0x92 /*Open A20.*/

   movl %cr0,%eax
   orl $0x1, %eax
   movl %eax,%cr0 /*Go into Protected Mode (but not true).*/

/*Go into Protected Mode true!*/
   .byte 0x66,0xea
   .long LOADER_MEMORY + protectedMode
   .word SelectorFlatC
/*It is the same as
    `jmp dword SectorFlatC:[LOAD_MEMORY + protectedMode] (nasm sytanx)*/

protectedMode: .code32
/*In Protected Mode!*/
   hlt
   jmp protectedMode

.include "pmode.inc.S"
.include "lmode.inc.S"
.include "boot.inc.S"

/*GDT*/
GDT:       GDTDescriptor 0x0, 0x0,     0x0
GDTFlatC: GDTDescriptor 0x0, 0xFFFFF, DA_CR | DA_32 | DA_LIMIT_4K
GDTFlatRW:GDTDescriptor 0x0, 0xFFFFF, DA_DRW | DA_LIMIT_4K

.set GDTLength    , . - GDT
.set SelectorFlatC, GDTFlatC - GDT

GDTR:.word GDTLength - 1
      .long LOADER_MEMORY + GDT
