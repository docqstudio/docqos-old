.section ".text","ax"
.extern exit
.extern main
.global _start
.code64

_start:
   call main
   movq %rax,%rdi
   call exit
   ud2
