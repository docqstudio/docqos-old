.global _start
.extern kinit
.extern kmain

.section ".magic"
.ascii "KERNEL"

.section ".text","ax"
.code64
_start:
   movq $stackTop,%rax
   movq %rax,%rsp

   callq kmain

   cli
0:
   hlt
   jmp 0b

.section ".bss"
stack:
.fill 0x1000,0x1,0x0
stackTop:

/*Kernel Page Tabel(NO PTE).*/
kernelPDEDir:
.fill 0x512,0x8,0x0

kernelPDPTEDir:
.fill 0x512,0x8,0x0

kernelPML4EDir:
.fill 0x512,0x8,0x0
