.global defaultInterruptHandler
.extern printk

.section ".data"

interruptOccurrent: .ascii "An interrupt is occurrent.\n"
          .byte 0
.section ".text","ax"
.code64

defaultInterruptHandler:
   pushq %rdi
   pushq %rbx
   pushq %rax
   pushq %rcx
   pushq %rdx
   movl $interruptOccurrent,%edi
   movl %edi,%ebx
   xorl %eax,%eax
   callq printk
   movl $0xFEE000B0,%eax
   movl $0,(%eax)
   popq %rdx
   popq %rcx
   popq %rax
   popq %rbx
   popq %rdi
   iretq

