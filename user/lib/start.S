.section ".text","ax"
.extern exit
.extern main
.global _start
.code64

_start:
   movq %rcx,%rdi
   movq %rbx,%rsi
   call main
   movq %rax,%rdi
   call exit
   ud2
