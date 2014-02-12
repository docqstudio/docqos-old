.section ".text","ax"
.extern exit
.extern main
.extern close
.global _start
.code64

_start:
   movq %rcx,%rdi
   movq %rbx,%rsi
   call main

   pushq %rax
   movq $0,%rax
   call close /*Close stdin.*/
   movq $1,%rax
   call close  /*Close stdout.*/
   popq %rax

   movq %rax,%rdi
   jmp exit
