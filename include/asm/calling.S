.macro SAVE_ALL
   pushq %rax
   pushq %rbx
   pushq %rcx
   pushq %rdx
   pushq %rbp
   pushq %rsi
   pushq %rdi
   pushq %r8
   pushq %r9
   pushq %r10
   pushq %r11
   pushq %r12
   pushq %r13
   pushq %r14
   pushq %r15
.endm

.macro RESTORE_ALL
   popq %r15
   popq %r14
   popq %r13
   popq %r12
   popq %r11
   popq %r10
   popq %r9
   popq %r8
   popq %rdi
   popq %rsi
   popq %rbp
   popq %rdx
   popq %rcx
   popq %rbx
   popq %rax
.endm
