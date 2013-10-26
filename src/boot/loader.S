.set inLoader,0
.set BOOTSEG , 0x7c0

.global _start

.section ".text" , "ax"
.code16

_start: .ascii "loader"
   jmp start

.include "pmode.inc.S"
.include "lmode.inc.S"
.include "boot.inc.S"

/*GDT*/
GDT:       GDTDescriptor 0x0, 0x0,     0x0
GDTNormal:GDTDescriptor 0x0, 0xFFFFF,DA_DRW
                                  /*It will be used when we return to Real Mode from Protected Mode*/
GDTFlatC16:GDTDescriptor 0x0, 0xFFFFF,DA_CR | DA_16 | DA_LIMIT_4K
GDTFlatC: GDTDescriptor 0x0, 0xFFFFF, DA_CR | DA_32 | DA_LIMIT_4K
GDTFlatC64:GDTDescriptor 0x0, 0xFFFF, DA_CR | DA_64 | DA_LIMIT_4K
GDTFlatRW:GDTDescriptor 0x0, 0xFFFFF, DA_DRW | DA_LIMIT_4K
GDTVideo: GDTDescriptor 0xB8000,0xFFFF,DA_DRW

.set GDTLength    , . - GDT
.set selectorNormal,GDTNormal - GDT
.set selectorFlatC16,GDTFlatC16 - GDT
.set selectorFlatC, GDTFlatC - GDT
.set selectorFlatC64,GDTFlatC64 - GDT
.set selectorFlatRW,GDTFlatRW - GDT
.set selectorVideo,GDTVideo - GDT

GDTR16:.word 0x0
       .long 0x0 /*It will be saved by `sgdt`.*/

GDTR:.word GDTLength - 1
      .long LOADER_MEMORY + GDT

GDTR64:.word GDTLength - 1
        .quad LOADER_MEMORY + GDT /*It is qword.*/

X86_64SupportError:
       .ascii "No support for X86_64.\n"
       .byte 0
lineFeed:.ascii "\n\r"
        .byte 0
goIntoProtectedMode: .ascii "Going into Protected Mode....\n\r"
        .byte 0
goIntoLongMode: .ascii "Going into Long Mode...\n\r"
         .byte 0
success:.ascii "Successful!\n\r"
        .byte 0

PTEMemory: .long PAGE_START_MEMORY
PDEMemory: .long 0x0
PDPTEMemory: .long 0x0
PML4EMemory: .long 0x0

espSaved: .long 0x0
esiSaved: .long 0x0
eaxSaved: .long 0x0

/*DiskAddressPacket (for int $0x13)*/
DAP: .skip 0x10

start:
   jmp entry32
entry32: .code16
   movw %cs,%ax
   movw %ax,%ds
   movw %ax,%gs
   movw %ax,%es /*Init segment registers again.*/

   movw $lineFeed,%bp
   call dispStrRM
   movw $goIntoProtectedMode,%bp
   call dispStrRM

   sgdt (GDTR16) /*Save GDT in Real Mode.*/
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
   .word selectorFlatC
/*It is the same as
    `jmp dword selectorFlatC:[LOAD_MEMORY + protectedMode] (nasm sytanx)*/

protectedMode: .code32
/*In Protected Mode!*/
   movw $selectorFlatRW,%ax
   movw %ax,%es
   movw %ax,%ds /*Init segment registers.*/

   movw %ax,%ss
   movl $(BOOTSEG*0x10),%esp /*Init ss and esp.*/

   movw $selectorVideo,%ax
   movw %ax,%gs
   movb $0x07,%ah
   movl $((80*4 + 0)*2),%esi /*Ready for displaying strings.*/

   movl $(success + LOADER_MEMORY),%ebp
   call dispStrPM
   movl $(goIntoLongMode + LOADER_MEMORY),%ebp
   call dispStrPM

   call checkX86_64Support
   cmpb $0,%al
   jne 1f /*If not support, `jmp` to 1f.*/

   movl $test,%eax
   call callRMFunctionInPM

   jmp entry64
0:
   hlt
   jmp 0b
1:
   movl $(LOADER_MEMORY + X86_64SupportError),%ebp
   call dispStrPM
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
                     /*gs = selector Video ,es:ebp = String */
                     /*ah = Color Code ,esi = Pos*/
1: /*label loop*/
   movb %es:(%ebp),%al
   cmpb $0,%al
   je 2f
   cmp $'\r',%al
   je 5f
   cmp $'\n',%al
   je 4f
   movw %ax,%gs:(%esi)
3: /*label next*/
   addl $0x2,%esi
5: /*label _next*/
   incl %ebp
   jmp 1b
2: /*label end*/
   ret
4: /*label line feed*/
   pushl %eax

   movl %esi,%eax
   movb $160,%bl
   divb %bl
   andl $0xFF,%eax /*Now eax = al = row number.*/

   incl %eax /*eax = new row number.*/
   mulb %bl
   movl %eax,%esi

   popl %eax

   subl $0x2,%esi /*Offset `add $0x2,%esi`.*/

   jmp 3b

dispStrLM: .code64 /*Display string in Long Mode.*/
                     /*rbx = Video Memory ,rbp = String */
                     /*ah = Color Code ,esi(rsi) = Pos*/
1: /*label loop*/
   movb (%rbp),%al
   cmpb $0,%al
   je 2f
   cmp $'\r',%al
   je 3f
   cmp $'\n',%al
   je 4f
   movw %ax,(%rbx,%rsi,1)
3: /*label next*/
   incq %rbp
   addq $0x2,%rsi
   jmp 1b
2: /*label end*/
   ret
4: /*label line feed*/
   pushq %rax

   movl %esi,%eax
   movb $160,%cl
   divb %cl
   andl $0xFF,%eax /*Now eax = al = row number.*/

   incl %eax /*eax = new row number.*/
   mulb %cl
   movl %eax,%esi

   popq %rax

   subq $0x2,%rsi /*Offset `add $0x2,%esi`.*/

   jmp 3b

callRMFunctionInPM:.code32 /*It will return to Real Mode and `call *%eax` */
                           /*Then it will go into Protected Mode again.*/
   movl %esp,(espSaved + LOADER_MEMORY)
   movl %esi,(esiSaved + LOADER_MEMORY)
   movl %eax,(eaxSaved + LOADER_MEMORY)

   ljmp $selectorFlatC16,$(3f + LOADER_MEMORY)
3:.code16

   movw $selectorNormal,%ax
   movw %ax,%gs
   movw %ax,%ds
   movw %ax,%es
   movw %ax,%ss
   movw %ax,%fs

   movl %cr0,%eax
   andb $0xFE,%al
   movl %eax,%cr0

   jmpl $(LOADER_MEMORY/0x10),$1f
1: .code16 /*label Real Mode*/
   movw %cs,%ax
   movw %ax,%ds
   movw %ax,%es
   movw %ax,%gs
   movw %ax,%fs

   inb $0x92,%al
   andb $0xFD,%al
   outb %al,$0x92 /*Close A20.*/

   lgdt (GDTR16)

   sti

   movw $0,%ax
   movw %ax,%ss
   movl (espSaved),%esp /*NOTE: %esp can not be more than 0xFFFF.Or this function will be failed!*/

   movl (eaxSaved),%eax
   call *%eax

   cli

   lgdt (GDTR)

   inb $0x92,%al
   orb $0x2,%al /*Open A20.*/
   outb %al,$0x92

   movl %cr0,%eax
   orl $0x1,%eax
   movl %eax,%cr0

   ljmp $selectorFlatC,$(LOADER_MEMORY + 2f)
2:.code32 /*label Protected Mode*/
   movw $selectorFlatRW,%ax
   movw %ax,%ds
   movw %ax,%es
   movw %ax,%fs
   movw %ax,%gs

   movw %ax,%ss
   movl (espSaved + LOADER_MEMORY),%esp

   movl (esiSaved + LOADER_MEMORY),%esi
   ret

test:.code16
   ret

checkX86_64Support:.code32 /*SMP and interrupt should be off!*/
   pushl %edx
   pushl %eax

   /*Check CPUID support. (486 supports but 386 doesn't)*/
   pushfl
   popl %eax
   movl %eax,%edx
   orl $EFLAGS_AC_BIT,%edx
   pushl %edx
   popfl

   pushfl
   popl %ecx
   cmpl %edx,%ecx
   jne 4f /*No support(cpuid).*/
   pushl %eax
   popfl

   movl $0x80000000,%eax
   cpuid /*Check extand cpuid support.*/
   cmpl $0x8000004,%eax
   jb 2f /*No support!*/

   movl $0x80000001,%eax
   cpuid
   btl $29,%edx
   jnc 3f /*Check X86_64 support.*/

1: /*label support*/
   popl %eax
   movb $0,%al
   popl %edx
   ret
2: /*label support extand cpuid error*/
   popl %eax
   movb $1,%al
   popl %edx
   ret
3: /*label support X86_64 error*/
   popl %eax
   movb $2,%al
   popl %edx
   ret
4: /*label 486 error*/
   pushl %eax
   popfl

   popl %eax
   movb $3,%al
   popl %edx
   ret

entry64: .code32
   call pagingInit

   movl (PML4EMemory + LOADER_MEMORY),%eax
   movl %eax,%cr3

   movl %cr4,%eax
   orl $0x30,%eax
   movl %eax,%cr4 /*Set %cr4.pse = %cr4.pae = 1.*/

   movl $0x0c0000080,%ecx
   rdmsr
   btsl $8,%eax
   wrmsr /*Set EFER.lme = 1.*/

   movl %cr0,%eax
   btsl $31,%eax
   movl %eax,%cr0 /*%cr0.pg = 1*/
   /*EFER.lma will set to 1 auto when %cr0.pg = 1 and EFER.lme = 1.*/
   /*Then we will go into Long Mode.*/

   jmp 1f
1:
   ljmp $selectorFlatC64,$(LOADER_MEMORY + longMode)

pagingInit: .code32 /*Init long mode paging.*/ /*Only mapping 4GB memory. But now it is enough!*/
   /*Start to init PTE.*/
   movl (PTEMemory + LOADER_MEMORY),%eax
   movl $((1024*1024*1024*4)/(4*1024)),%ecx
   movl $(0 + 0x3),%edi
1: /*label PTE loop*/
   movl %edi,(%eax)
   movl $0,4(%eax)
   addl $0x8,%eax
   addl $0x1000,%edi
   loop 1b

   /*Start to init PDE.*/
   movl %eax,(PDEMemory + LOADER_MEMORY)
   movl $((1024*1024*1024*4)/(2*1024*1024)),%ecx
   movl (PTEMemory + LOADER_MEMORY),%edi
   addl $0x3,%edi
2: /*label PDE loop*/
   movl %edi,(%eax)
   movl $0,4(%eax)
   addl $0x8,%eax
   loop 2b

   /*Start to init PDPTE.*/
   movl %eax,(PDPTEMemory + LOADER_MEMORY)
   movl $((1024*1024*1024*4)/(1024*1024*1024)),%ecx
   movl (PDEMemory + LOADER_MEMORY),%edi
   addl $0x3,%edi
3: /*label PDPTE loop*/
   movl %edi,(%eax)
   movl $0,4(%eax)
   addl $0x8,%eax
   loop 3b

   movl $(512 - ((1024*1024*1024*4)/(1024*1024*1024))),%ecx
   movl $0x0,%edi
4: /*label PDPTE empty loop*/
   movl %edi,(%eax)
   movl $0,4(%eax)
   addl $0x8,%eax
   loop 4b

   /*Start to init PML4E.*/
   movl %eax,(PML4EMemory + LOADER_MEMORY)
   movl (PDPTEMemory + LOADER_MEMORY),%edi
   addl $0x3,%edi
   movl %edi,(%eax)
   movl $0,4(%eax)
   addl $0x8,%eax

   movl $0x0,%edi
   movl $(512 - 1),%ecx
5: /*label PML4E empty loop*/
   movl %edi,(%eax)
   movl $0,4(%eax)
   addl $0x8,%eax
   loop 5b
   ret

longMode: .code64
   lgdt (LOADER_MEMORY + GDTR64) /*Load 64-bit gdt.*/

   movw $selectorFlatRW,%ax
   movw %ax,%ds
   movw %ax,%es
   movw %ax,%ss
   movw %ax,%gs /*Init segment registers.*/

   movq %rsp,%rax
   pushq $selectorFlatRW
   pushq %rax
   pushfq
   pushq $selectorFlatC64
   pushq $(2f + LOADER_MEMORY)
   iretq /*Init cs.*/

2: /*label next*/
   movq $0xB8000,%rbx
   movb $0x07,%ah
   //movq $((80*2 + 0)*2),%rsi /*Ready for displaying strings.*/

   movq $(success + LOADER_MEMORY),%rbp
   callq dispStrLM

1: /*label loop*/
   hlt
   jmp 1b

