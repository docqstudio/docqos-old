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

   pushq %rax /*Save the retval of the main function.*/
   
   movq $0,%rdi
   call close /*Close stdin.*/
   movq $1,%rdi
   call close /*Close stdout.*/
   movq $2,%rdi
   call close /*Close stderr.*/

   popq %rdi /*Restore the retval to %rdi.*/

   jmp exit /*Call exit(%rdi) .*/
