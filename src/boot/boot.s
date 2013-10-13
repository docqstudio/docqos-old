BOOTSEG=0x7c0
.section .text
.code16
.global _start
_start: /*DL = driver number for CD (boot from CD) */
   jmpl $BOOTSEG,$start
start:
   movw %cs,%ax
   movw %ax,%es
   movw %ax,%ds
   movw %ax,%fs
   movw %ax,%gs /*Init segment register.*/

   movw $BOOTSEG,%ax
   movw %ax,%ss
   movw $0,%sp /*Init ss and sp*/

   movb %dl,(driveNumber) /*Save driver number for CD.*/

   movw $DAP,%si
   movb $0x10,(%si)
   movw $LOADER_SIZE_SECTOR,2(%si)
   movw $LOADER_MEMORY_OFF,4(%si)
   movw $LOADER_MEMORY_SEG,6(%si)
   movl $0x0,8(%si) /*Init DAP (for int $0x13).*/

   pushw $LOADER_MEMORY_SEG
   popw %gs
   movw $LOADER_MEMORY_OFF,%di /*gs:di = the memory that loader will load.*/

.restart:
   call readSectors
   movw $DAP,%bp
   incl 8(%bp) /*Add one to LBA of DAP.*/

   cmpb $0,%ah
   je .sucess
.error:
   movw $noloader,%bp
   call dispStr /*Display "No loader."*/
   jmp .fin
   
.sucess:
   movw $LOADER_MAGIC_STRING,%bp
   movw $LOADER_MEMORY_OFF,%di
   call cmpStr /*Check loader.*/
   cmpw $0x0,%ax
   je .restart /*If it isn't loader,`jmp` to .restart*/

   movw $loader,%bp
   call dispStr

   movw $farJmp,%si
   movw %di,(%si)
   movw %gs,2(%si)
   ljmp *farJmp
   
.fin:
   hlt
   nop
   jmp .fin

readSectors:/*int $0x13*/

   movb $0x42,%ah /*Extend read*/
   movb (driveNumber),%dl /*Driver number for CD*/

   movw $DAP,%si
   pushw $BOOTSEG
   popw %ds
   /*ds:si = DiskAddressPacket*/

   int $0x13

   ret

dispStr: /*Display string*/ /*es:bp = string*/
   movw $15,%bx
   movb $0xe,%ah
.loop:
   movb %es:(%bp),%al
   cmpb $0,%al
   je .end
   int $0x10
   incw %bp
   jmp .loop
.end:
   ret

cmpStr: /*Compare es:bp with gs:di*/
.loop_:
   movb %es:(%bp),%al
   cmpb $0,%al
   je .yes
   movb %gs:(%di),%ah
   cmpb %ah,%al
   jne .not
   incw %bp
   incw %si
   jmp .loop_
.not:
   movw $0,%ax
   ret
.yes:
   movw $1,%ax
   ret

/*DiskAddressPacket (for int $0x13)*/
DAP: .long 0x0
     .long 0x0
     .long 0x0
     .long 0x0

loader: .ascii "loader"
        .byte 0xa,0xd,0x0
noloader: .ascii "No loader."
     .byte 0xa,0xd,0x0
driveNumber: .byte 0x0

farJmp: .int 0x0
        .int 0x0

.include "boot.inc.s"
