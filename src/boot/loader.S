.set inLoader,0
.set BOOTSEG , 0x7c0

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
    `jmp dword SelectorFlatC:[LOAD_MEMORY + protectedMode] (nasm sytanx)*/

protectedMode: .code32
/*In Protected Mode!*/
   movw %cs,%ax
   movw %ax,%es
   movw %ax,%ds /*Init segment registers.*/

   movw $SelectorFlatRW,%ax
   movw %ax,%ss
   movl $(BOOTSEG*0x10),%esp /*Init ss and esp.*/

   movw $SelectorVideo,%ax
   movw %ax,%gs

   movb $0x07,%ah
   movl $(LOADER_MEMORY + test),%ebp
   movl $((80*2 + 0)*2),%esi
   call dispStrPM
0:
   hlt /*Now interrupt is closed,so it will nerver exit!*/
   jmp 0b

dispStrRM: .code16 /*Display string(es:bp) in Real Mode.*/
                     /*It is the same as dispStr in boot.S*/
   movw $15,%bx
   movb $0xe,%ah
1: /*label loop*/
   movb %es:(%bp),%al
   cmpb $0,%al
   je 2f
   int $0x10
   incw %bp
   jmp 1b
2: /*label end*/
   ret

dispStrPM: .code32 /*Display string in Protected Mode.*/
                     /*gs = Selector Video ,es:ebp = String */
                     /*ah = Color Code ,esi = Pos*/
1: /*label loop*/
   movb %es:(%ebp),%al
   cmp $0,%al
   je 2f
   movw %ax,%gs:(%esi)
   incl %ebp
   add $2,%esi
   jmp 1b
2: /*label end*/
   ret
.include "pmode.inc.S"
.include "lmode.inc.S"
.include "boot.inc.S"

/*GDT*/
GDT:       GDTDescriptor 0x0, 0x0,     0x0
GDTFlatC: GDTDescriptor 0x0, 0xFFFFF, DA_CR | DA_32 | DA_LIMIT_4K
GDTFlatRW:GDTDescriptor 0x0, 0xFFFFF, DA_DRW | DA_LIMIT_4K
GDTVideo: GDTDescriptor 0xB8000,0xFFFF,DA_DRW

.set GDTLength    , . - GDT
.set SelectorFlatC, GDTFlatC - GDT
.set SelectorFlatRW,GDTFlatRW - GDT
.set SelectorVideo,GDTVideo - GDT

GDTR:.word GDTLength - 1
      .long LOADER_MEMORY + GDT

test: .ascii "In loader Protected Mode!!!!!"
       .byte 0
