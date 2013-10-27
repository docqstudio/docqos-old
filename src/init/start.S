.global _start
.section ".magic"
.ascii "KERNEL"
.section ".text","ax"
.code64
_start:
   hlt
   jmp _start
