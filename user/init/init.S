.section ".text","ax"
.code64
.global _start

_start:
   movq $1,%rax /*NR_OPEN.*/
   movq $ttyFile,%rbx /*Filename: "/dev/tty".*/
   int $0xff /*int fd = open("/dev/tty");*/

   movq %rax,%rbx /*Fd.*/
   movq $2,%rax /*NR_WRITE*/
   movq $info,%rcx /*Buffer: "Hello!!".*/
   movq $0,%rdx /*Length. (Will be ingnored by kenrel.)*/
   int $0xff /*write(fd,"Hello!!",0);*/

   movq %rbx,%rbx /*Fd.*/
   movq $3,%rax /*NR_CLOSE.*/
   int $0xff /*close(fd).*/
0:
   jmp 0b /*End-less loop.*/
ttyFile:.ascii "/dev/tty"
        .byte 0
info: .ascii "Hello,world!"
      .byte 0
