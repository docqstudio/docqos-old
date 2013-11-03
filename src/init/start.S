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
.rept 0x1000
.byte 0x0
.endr
stackTop:
