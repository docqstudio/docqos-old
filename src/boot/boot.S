.set inBoot , 0
.set BOOTSEG , 0x7c0

.global _start

.section ".text" ,"ax"
.code16
_start: /*DL = driver number for CD (boot from CD) */
   jmpl $BOOTSEG,$start
start:
   movw %cs,%ax
   movw %ax,%es
   movw %ax,%ds
   movw %ax,%fs
   movw %ax,%gs /*Init segment registers.*/

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

   movw $LOADER_MEMORY_SEG,%ax
   movw %ax,%gs
   movw $LOADER_MEMORY_OFF,%di /*gs:di = the memory that loader will load.*/

   call clearScreen

   movw $loading,%bp
   call dispStr

   call checkExtandInt13HSupport
   jc 5f

1: /*label restart*/
   call readSectors
   movw $DAP,%bp
   incl 8(%bp) /*Add one to LBA of DAP.*/

   cmpb $0,%ah
   je 3f /*f = forward*/
2: /*label error*/
   movw $noloader,%bp
   call dispStr /*Display "No loader."*/
   jmp 4f
   
3: /*label success*/
   movw $LOADER_MAGIC_STRING,%bp
   movw $LOADER_MEMORY_OFF,%di
   call cmpStr /*Check loader.*/
   cmpw $0x0,%ax
   je 1b /*If it isn't loader,`jmp` to restart(1b [b = backward])*/

   movw $success,%bp
   call dispStr

   movw $farJmp,%si
   movw %di,(%si)
   movw %gs,2(%si)

/*************************************/
   ljmp *farJmp     /*GO INTO LOADER!*/
/*************************************/

   
4: /*label fin*/
   hlt
   nop
   jmp 4b

5: /*label extand int $0x13 error*/
   movw $int13HSupportError,%bp
   call dispStr
   jmp 4b

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
1: /*label loop*/
   movb %es:(%bp),%al
   cmpb $0,%al
   je 2f
   int $0x10
   incw %bp
   jmp 1b
2: /*label end*/
   ret

cmpStr: /*Compare es:bp with gs:di*/
1: /*label loop*/
   movb %es:(%bp),%al
   cmpb $0,%al
   je 3f
   movb %gs:(%di),%ah
   cmpb %ah,%al
   jne 2f
   incw %bp
   incw %di
   jmp 1b
2: /*label not*/
   movw $0,%ax
   ret
3: /*label yes*/
   movw $1,%ax
   ret

clearScreen:
   movb $0x6,%ah
   movb $0x0,%al
   movw $0x0,%cx
   movb $24,%dh
   movb $79,%dl
   movb $0x7,%bh
   int $0x10 /*Clear the screen.*/

   movb $0x2,%ah
   movb $0x0,%bh
   movw $0x0,%dx
   int $0x10 /*Move the cursor to 0,0.*/
   ret

checkExtandInt13HSupport:
   movb $0x41,%ah
   movw $0x55AA,%bx
   movb (driveNumber),%dl
   int $0x13
   ret

/*DiskAddressPacket (for int $0x13)*/
DAP: .skip 0x10

loading: .ascii "Loading loader...."
         .byte 0xa,0xd,0x0
success: .ascii "Successful!"
        .byte 0xa,0xd,0x0
noloader: .ascii "No loader."
     .byte 0xa,0xd,0x0
int13HSupportError: .ascii "No support for extand int $0x13"
      .byte 0xa,0xd,0x0
driveNumber: .byte 0x0

farJmp: .word 0x0
        .word 0x0

.include "boot.inc.S"

.org 510
.word 0xAA55
