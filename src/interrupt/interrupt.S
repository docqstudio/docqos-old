.global defaultInterruptHandler
.global irqHandlers
.global handleLocalApicTimer

.extern localApicSendEOI
.extern localApicTimerHandler
.extern doIRQ
.extern printk

.include "asm/calling.S"

.section ".data"

interruptOccurrent: .ascii "An interrupt is occurrent.\n"
          .byte 0

.section ".text","ax"
.code64

defaultInterruptHandler:
   SAVE_ALL

   movabs $interruptOccurrent,%rdi
   movq %rdi,%rbx
   xorq %rax,%rax
   callq printk /*Printk "An interrupt is occurrent.\n".*/

   call localApicSendEOI
 
   RESTORE_ALL
   iretq

.macro irq index
handleIRQ\index:
   pushq $\index
   jmp irqHandler
.endm

irq 0
irq 1
irq 2
irq 3
irq 4
irq 5
irq 6
irq 7
irq 8
irq 9
irq 10
irq 11
irq 12
irq 13
irq 14
irq 15
irq 16
irq 17
irq 18
irq 19
irq 20
irq 21
irq 22
irq 23


irqHandler:
   SAVE_ALL

   call doIRQ

   RESTORE_ALL
   addq $8,%rsp /*Skip the interrupt vector.*/
   iretq

irqHandlers:
   .quad handleIRQ0
   .quad handleIRQ1
   .quad handleIRQ2
   .quad handleIRQ3
   .quad handleIRQ4
   .quad handleIRQ5
   .quad handleIRQ6
   .quad handleIRQ7
   .quad handleIRQ8
   .quad handleIRQ9
   .quad handleIRQ10
   .quad handleIRQ11
   .quad handleIRQ12
   .quad handleIRQ13
   .quad handleIRQ14
   .quad handleIRQ15
   .quad handleIRQ16
   .quad handleIRQ17
   .quad handleIRQ18
   .quad handleIRQ19
   .quad handleIRQ20
   .quad handleIRQ21
   .quad handleIRQ22
   .quad handleIRQ23

handleLocalApicTimer:
   pushq $0xfe
   SAVE_ALL

   call localApicSendEOI

   movabs $localApicTimerHandler,%rax
   movq (%rax),%rax
   cmpq $0,%rax
   je 1f /*Check if we have a handler.*/
   call *%rax

1:
   RESTORE_ALL
   addq $8,%rsp
   iretq
