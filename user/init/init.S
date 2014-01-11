.section ".text","ax"
.code64
.extern main
.global _start

_start:
   call main
0:
   jmp 0b /*End-less loop.*/
