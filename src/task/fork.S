.include "asm/calling.S"
.extern doExit
.extern finishScheduling

.global kernelTaskHelper
.global retFromFork
/*.global retFromSyscallFork*/

.section ".text"
.code64

retFromFork:
   movq %rax,%rdi
   call finishScheduling
   RESTORE_ALL
   addq $8,%rsp
   iretq

kernelTaskHelper:
   callq *%rbx
   movq %rax,%rdi
   callq doExit
   ud2
