BOOTSEG=0x1000
.section .text
.code16
.global _start
_start:
   hlt
   nop
   jmp _start
