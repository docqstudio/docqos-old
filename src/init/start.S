.section ".magic"
.ascii "KERNEL"
.quad _start

.section ".text","ax"
.code64
.global _start
.extern kmain

_start:
   movabs $stackTop,%rax
   movq %rax,%rsp

   callq kmain

0:
   hlt
   jmp 0b

.section ".bss","aw"
.code32
stack:
.fill 0x1000,0x1,0x0
stackTop:
