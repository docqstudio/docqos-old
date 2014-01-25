.extern handleException
.global exceptionHandlers

.section ".text","ax"
.code64

.macro exceptionWithoutErrorCode index
exceptionHandler\index:
   subq $8,%rsp
   pushq %rax
   movabs $(\index << 56),%rax
   movq %rax,8(%rsp)
   popq %rax
   jmp exception
.endm
.macro exceptionWithErrorCode index
exceptionHandler\index:
   pushq %rax
   movabs $(\index << 56),%rax
   addq %rax,8(%rsp)
   popq %rax
   jmp exception
.endm

.include "asm/calling.S"

exceptionWithoutErrorCode 0
exceptionWithoutErrorCode 1
exceptionWithoutErrorCode 2
exceptionWithoutErrorCode 3
exceptionWithoutErrorCode 4
exceptionWithoutErrorCode 5
exceptionWithoutErrorCode 6
exceptionWithoutErrorCode 7
exceptionWithErrorCode 8
exceptionWithoutErrorCode 9
exceptionWithErrorCode 10
exceptionWithErrorCode 11
exceptionWithErrorCode 12
exceptionWithErrorCode 13
exceptionWithErrorCode 14
exceptionWithoutErrorCode 15
exceptionWithoutErrorCode 16
exceptionWithErrorCode 17
exceptionWithoutErrorCode 18
exceptionWithoutErrorCode 19

exception:
   SAVE_ALL

   movq %rsp,%rdi
   callq handleException
   
   RESTORE_ALL
   addq $8,%rsp
   iretq

exceptionHandlers:
   .quad exceptionHandler0
   .quad exceptionHandler1
   .quad exceptionHandler2
   .quad exceptionHandler3
   .quad exceptionHandler4
   .quad exceptionHandler5
   .quad exceptionHandler6
   .quad exceptionHandler7
   .quad exceptionHandler8
   .quad exceptionHandler9
   .quad exceptionHandler10
   .quad exceptionHandler11
   .quad exceptionHandler12
   .quad exceptionHandler13
   .quad exceptionHandler14
   .quad exceptionHandler15
   .quad exceptionHandler16
   .quad exceptionHandler17
   .quad exceptionHandler18
   .quad exceptionHandler19
