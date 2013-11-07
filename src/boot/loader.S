.set inLoader,0
.set BOOTSEG , 0x7c0

.global _start

.section ".text" , "ax"
.code16

_start: .ascii "LOADER"
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

.set GDTLength    , . - GDT
.set selectorNormal,GDTNormal - GDT
.set selectorFlatC16,GDTFlatC16 - GDT
.set selectorFlatC, GDTFlatC - GDT
.set selectorFlatC64,GDTFlatC64 - GDT
.set selectorFlatRW,GDTFlatRW - GDT

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
noKernel: .ascii "No kernel!\n\r"
         .byte 0
vbeInitFailed: .ascii "Initing VBE failed!\n\r"
              .byte 0
readKernelFailed: .ascii "Reading kernel failed!\n\r"
          .byte 0

PTEMemory: .long PAGE_START_MEMORY
PDEMemory: .long 0x0
PDPTEMemory: .long 0x0
PML4EMemory: .long 0x0

espSaved: .long 0x0
eaxSaved: .long 0x0

driveNumber: .byte 0x0
kernelSectorNumber:.long 0x0

sectorSize: .word 0x0
kernelSeekSector: .long 0x0

/*DiskAddressPacket (for int $0x13)*/
DAP: .skip 0x10
/*DriveParametersPacket*/
DPP: .skip 0x1E

start:
   movw %cs,%ax
   movw %ax,%ds
   movw %ax,%gs
   movw %ax,%es /*Init segment registers again.*/

   movb %dl,(driveNumber)

   movw $DAP,%si
   movb $0x10,(%si)
   movw $0x1,2(%si)
   movw $KERNEL_MEMORY_OFF,4(%si)
   movw $KERNEL_MEMORY_SEG,6(%si)
   movl $0x0,8(%si)
   movl $0x0,12(%si)/*Init DAP (for int $0x13).*/


   movw $lineFeed,%bp
   call dispStrRM

   call driveSectorSize
   cmpw $0,%ax
   je 1f
   movw %ax,(sectorSize)

   call searchKernel
   jc 1f

   pushl %eax
   call checkX86_64Support
   cmpb $0,%al
   jne 4f /*If not support, `jmp` to 1f.*/

   call getVGAFonts
   popl %eax
   jmp 3f

1: /*label nokernel*/
   movw $noKernel,%bp
   call dispStrRM
   cli
2: /*label fin*/
   hlt
   jmp 2b
3:/*label next*/
   movl %eax,(kernelSectorNumber)
   jmp entry32
4: /*label x86_64 support error*/
   popl %eax
   movw $X86_64SupportError,%bp
   call dispStrRM
   jmp 2b
entry32: .code16
   movw $goIntoProtectedMode,%bp
   call dispStrRM

   call vbeInit
   jc 1f

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
 1: /*label failed to init VBE*/
    movw $vbeInitFailed,%bp
    call dispStrRM
    cli
 2: /*label fin*/
    hlt
    jmp 1b

searchKernel: .code16
   movw $KERNEL_MAGIC_STRING,%bp
   movw $KERNEL_MEMORY_SEG,%ax
   movw %ax,%gs
   movw $KERNEL_MEMORY_OFF,%di
   movw $DAP,%si

1: /*label loop*/
   incl 8(%si)
   call readSectors
   jc 2f

   call cmpStr
   cmpw $0x0,%ax
   clc
   je 1b

   movl 8(%si),%eax
   ret
2: /*label error*/
   stc
   ret

cmpStr:.code16 /*Compare es:bp with gs:di*/
1: /*label loop*/
   movb %es:(%bp),%al
   cmpb $0,%al
   je 3f
   movb %gs:(%di),%ah
   cmpb %ah,%al
   jne 2f
   incw %bp
   incw %di
   jmp 1b
2: /*label not*/
   movw $0,%ax
   ret
3: /*label yes*/
   movw $1,%ax
   ret

readSectors: .code16 /*int $0x13*/

   movb $0x42,%ah /*Extend read*/
   movb (driveNumber),%dl /*Driver number for CD*/

   movw $DAP,%si
   /*ds:si = DiskAddressPacket*/

   int $0x13

   ret

readKernel: .code16
   movl (kernelSectorNumber),%eax
   addl (kernelSeekSector),%eax
   movl $KERNEL_SIZE_SECTOR,%ebx
   subl (kernelSeekSector),%ebx
   cmpl %ebx,%ecx
   jnc 2f

3:/*label next*/
   movw $DAP,%si
   movb $0x10,(%si)
   movw %cx,2(%si)
   movw $KERNEL_MEMORY_OFF,4(%si)
   movw $KERNEL_MEMORY_SEG,6(%si)
   movl %eax,8(%si)
   movl $0x0,12(%si)/*Init DAP again.*/

   call readSectors
   jc 1f
   addl %ecx,(kernelSeekSector)
   ret
1:/*label error*/
   movw $readKernelFailed,%bp
   call dispStrRM
   ret
2: /*label more than*/
   movl %ebx,%ecx
   jmp 3b

driveSectorSize: .code16
   movb (driveNumber),%dl
   movw $DPP,%si
   movb $0x48,%ah

   movw $0x1E,(%si)

   int $0x13
   jc 1f
   movw 0x18(%si),%ax /*%ax = sector size*/
   ret
1: /*label error*/
   movw $0,%ax
   ret

vbeInit: .code16
   pushw %es

   movw $VBE_INFO_MEMORY_SEG,%ax
   movw %ax,%es
   movw $0x0,%di
   movw $0x4F00,%ax
   int $0x10 /*Check support for VBE.*/
   cmpw $0x4f,%ax
   jne 1f

   movw %es:4(%di),%ax
   cmpw $0x0200,%ax /*VBE2.0 or more.*/
   jb 1f

   movw $VBE_MODE_INFO_MEMORY_SEG,%ax
   movw %ax,%es
   movw $0x0,%di
   movw $VBE_DEFAULT_MODE,%cx
   movw $0x4F01,%ax
   int $0x10 /*Check support for VBE_DEFAULT_MODE.*/
   cmpw $0x4f,%ax
   jne 1f

   movw %es:0x0(%di),%ax
   andw $0x90,%ax
   cmpw $0x90,%ax
   jne 1f

   movb %es:0x19(%di),%al
   cmpb $0x18,%al
   jne 1f

   movb %es:0x1b(%di),%al
   cmpb $0x06,%al
   jne 1f

   movw $(VBE_DEFAULT_MODE + 0x4000),%bx
   movw $0x4f02,%ax
   int $0x10 /*Enable VBE_DEFAULT_MODE.*/
   popw %es

   clc
   ret
1: /*label error*/
   popw %es
   stc
   ret

getVGAFonts: .code16
   pushw %ds
   pushw %es

   movw $0x1130,%ax
   movb $6,%bh
   int $0x10

   movw %es,%ax
   movw %ax,%ds
   movw %bp,%si
   movw $(256*16/4),%cx
   movw $VGA_BITMAP_FONTS_MEMORY_SEG,%ax
   movw %ax,%es
   movw $0,%di
   rep movsd
   popw %es
   popw %ds
   ret

checkX86_64Support:.code16 /*SMP and interrupt should be off!*/
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
   jne 4f /*No support(cpuid).*
   pushl %eax
   popfl*/

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


protectedMode: .code32
/*In Protected Mode!*/
   movw $selectorFlatRW,%ax
   movw %ax,%es
   movw %ax,%ds /*Init segment registers.*/
   movw %ax,%gs

   movw %ax,%ss
   movl $(BOOTSEG*0x10),%esp /*Init ss and esp.*/

   call readAndInitKernel

   jmp entry64
0: /*label fin*/
   hlt
   jmp 0b

readAndInitKernel: .code32
   movl $KERNEL_REAL_MEMORY,%ebp /*10MB*/
   movl $KERNEL_MEMORY,%edi
   movl (sectorSize + LOADER_MEMORY),%ebx
1: /*label loop*/
   movl $readKernel,%eax
   movl $0x1,%ecx
   call callRMFunctionInPM
   movl %ebx,%eax
   mull %ecx
   movl %eax,%ecx
   call memcpy
   addl %eax,%ebp
   cmpl $KERNEL_SIZE_SECTOR,(kernelSeekSector + LOADER_MEMORY)
   je 2f
   jmp 1b
2: /*label end*/
   ret

memcpy:.code32 /*Copy es:edi to ds:ebp (size:ecx).*/
   pushl %eax
   pushl %edi
   pushl %ebp

1: /*label loop*/
   movb %es:(%edi),%al
   movb %al,%ds:(%ebp)
   incl %ebp
   incl %edi
   loop 1b

   popl %ebp
   popl %edi
   popl %eax
   ret

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

callRMFunctionInPM:.code32 /*It will return to Real Mode and `call *%eax` */
                           /*Then it will go into Protected Mode again.*/
   pushl %esi
   pushl %ebx
   movl %esp,(espSaved + LOADER_MEMORY)
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
   /*There don't need to add LOADER_MEMORY,because of ds register.*/
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
   popl %ebx
   popl %esi
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
   addl $0x1000,%edi
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
   addl $0x1000,%edi
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

strlen:.code64 /*Get length of string %rbp*/
   movl $0,%ecx
1:
   movb (%rbp),%al
   cmpb $0,%al
   je 2f
   incl %ecx
   incq %rbp
   jmp 1b
2:
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
   xorq %rcx,%rcx
   movl $(LOADER_MEMORY + KERNEL_MAGIC_STRING),%ebp
   call strlen
   movq $(KERNEL_ENTRY_ADDRESS),%rax
   addq %rcx,%rax /*Now %rax is kernel's address.*/

/************************************************/
   jmp *%rax  /*Go into kernel!!*/
/************************************************/
