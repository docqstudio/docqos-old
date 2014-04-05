.section ".text","ax"
.global __vkernelStartAddress
.global __vkernelSignalReturn
.global __vkernelSyscallEntry
.global __vkernelEndAddress
.code64

__vkernelStartAddress:
__vkernelSignalReturn:
   movq $20,%rax /*Signal Return.*/
   int $0xff /*Call the syscall.*/
   ud2 /*Should never arrive here.*/

__vkernelSyscallEntry:
   movq $-38,%rax /*-ENOSYS.*/
   ret
__vkernelEndAddress:
    /*See also vkernel.c .*/
