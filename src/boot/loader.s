.section .text
.code16
.global _start

_start: .ascii "loader"
start:
   hlt
   nop
   jmp start
