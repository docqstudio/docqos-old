.section ".boot","ax"
.global _start32
.extern multibootLoadEnd
.extern multibootAllEnd
.code32

.set PAGE_OFFSET            ,0x8000000000
.set BOOT_PAGE_PML4E_ADDRESS,0x60000
.set BOOT_PAGE_PDPTE_ADDRESS,0x62000
.set BOOT_PAGE_PDE_ADDRESS  ,0x64000

multibootHeaderStart:
.long 0xe85250d6 /*Multiboot2 Magic Number.*/
.long 0          /*Arch:i386 .*/
.long (multibootHeaderEnd - multibootHeaderStart) /*Multiboot Header Size.*/
.long -(0xe85250d6 + 0 + multibootHeaderEnd - multibootHeaderStart)
                                   /*Check Sum.*/
.word 2,0      /*Address Tag.*/
.long 24       /*Size.*/
.long multibootHeaderStart /*Header Address.*/
.long multibootHeaderEnd   /*Load Address*/
.long multibootLoadEnd     /*Load End Address.*/
.long multibootAllEnd      /*Bss End Address.*/

.word 3,0        /*Entry Address Tag.*/
.long 12         /*Size.*/
.long _start32   /*Entry Address.*/
.long 0          /*Align.*/

.word 5,0        /*Frame Buffer Tag.*/
.long 20         /*Size.*/
.long 1024       /*Width.*/
.long 768        /*Height.*/
.long 32         /*Depth.*/
.long 0          /*Align.*/

.word 0,0        /*End Tag.*/
.long 8          /*Size.*/

multibootHeaderEnd:

bootGDT32:
.quad 0x0000000000000000 /*Null Segment.*/
.quad 0x00cf9a000000ffff /*Code Segment.*/
.quad 0x00cf92000000ffff /*Data Segment.*/
bootGDT64:
.quad 0x0000000000000000 /*Null Segment.*/
.quad 0x00a09a0000000000 /*64-bit Code Segment.*/
.quad 0x00a0920000000000 /*64-bit Data Segment.*/

bootGDTR32:.word 23
           .long bootGDT32
bootGDTR64:.word 23
           .quad bootGDT64
bootGDTR64Paging:.word 32
                 .quad (bootGDT64 + 0x8000000000)

_start32:
   pushl $0
   pushl %eax  /*pushq %rax*/
   pushl $0
   pushl %ebx  /*pushq %rbx*/

   pushl $0
   popfl   /*Reset %rflags.*/

   lgdt (bootGDTR32) /*Load the boot 32-bit GDTR.*/
   ljmp $0x8,$1f

1:
   movw $0x10,%ax
   movw %ax,%es
   movw %ax,%gs
   movw %ax,%fs
   movw %ax,%ds
   movw %ax,%ss  /*Init segment registers.*/

   /*Init PDE. (2MB pages.)*/
   movl $BOOT_PAGE_PDE_ADDRESS,%eax
   movl $(1024 * 1024 * 0 + 0x83),  (%eax)
   movl $(1024 * 1024 * 2 + 0x83), 8(%eax)
   movl $(1024 * 1024 * 4 + 0x83),16(%eax)
   movl $(1024 * 1024 * 6 + 0x83),24(%eax)
   movl $(1024 * 1024 * 8 + 0x83),32(%eax)

   movl $0, 4(%eax)
   movl $0,12(%eax)
   movl $0,20(%eax)
   movl $0,28(%eax)
   movl $0,36(%eax)

   /*Init PDPTE.*/
   orl $3,%eax
   movl $BOOT_PAGE_PDPTE_ADDRESS,%ebx
   movl %eax,(%ebx)
   movl $0,4(%ebx)

   /*Init PML4E.*/
   orl $3,%ebx
   movl $BOOT_PAGE_PML4E_ADDRESS,%eax
   movl %ebx, (%eax)
   movl %ebx,8(%eax)
   movl $0, 4(%eax)
   movl $0,12(%eax)

   /*Load the page table..*/
   movl %eax,%cr3
   
   movl %cr4,%eax
   orl $(1 << 5),%eax
   movl %eax,%cr4  /*Enable PAE.*/

   movl $0xc0000080,%ecx
   rdmsr
   orl $(1 << 8),%eax
   wrmsr          /*Enable */

   movl %cr0,%eax
   orl $(1 << 31),%eax
   movl %eax,%cr0 /*Enable paging.*/

   lgdt (bootGDTR64)  /*Load the boot 64-bit GDTR.*/
   ljmp $0x8,$2f
2: .code64
   movl %esp,%eax
   xorq %rsp,%rsp
   movl %eax,%esp

   lgdt (bootGDTR64Paging) /*Load the boot 64-bit GDTR with paging.*/

   popq %rbx
   popq %rax
   
   movabs $PAGE_OFFSET,%rcx
   addq %rcx,%rbx
   movabs $_start64,%rcx
   jmp *%rcx 

.section ".text","ax"
.code64
.extern kmain

_start64:
   movabs $stackTop,%rcx
   movq %rcx,%rsp

   movq %rax,%rdi
   movq %rbx,%rsi
   callq kmain   /*Call kmain(%rax,%rbx).*/

0:
   hlt
   jmp 0b

.section ".bss","aw"
.code32
stack:
.fill 0x1000,0x1,0x0
stackTop:
