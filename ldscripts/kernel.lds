OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")  
OUTPUT_ARCH(i386:x86-64)  
ENTRY(_start32)  

KERNEL_PAGE_OFFSET = 0x8000000000;

SECTIONS {
   . = 0x0000100000;
   .boot : {
      *(.boot)
   }
   . = ALIGN(8);

   . += KERNEL_PAGE_OFFSET;
   .text : AT( ADDR(.text) - KERNEL_PAGE_OFFSET) {
      *(.text*)
   }
   . = ALIGN(8);
   .rodata : AT( ADDR(.rodata) - KERNEL_PAGE_OFFSET) {
      *(.rodata*)
   }
   . = ALIGN(8);
   .data : AT( ADDR(.data) - KERNEL_PAGE_OFFSET) {
      *(COMMON)
      *(.data)
   }
   . = ALIGN(8);

   exceptionFixUpTableStart = .;
   .fixup : AT( ADDR(.fixup) - KERNEL_PAGE_OFFSET) {
      *(.fixup)
   }
   exceptionFixUpTableEnd = .;

   . = ALIGN(8);

   initcallStart = .;

   .init.initcall : AT( ADDR(.init.initcall) - KERNEL_PAGE_OFFSET){
      *(.init.initcall0)
      *(.init.initcall1)
      *(.init.initcall2)
      *(.init.initcall3)
   }
   initcallEnd   = .;
   multibootLoadEnd = . - 0x8000000000;

   . = ALIGN(8);
   .bss : AT( ADDR(.bss) - KERNEL_PAGE_OFFSET){
      *(.bss)
   }
   . = ALIGN(8);

   endAddressOfKernel = .;
   . += 8;
   multibootAllEnd = . - 0x8000000000;

/*   .info.debug : { *(.debug*) }

   /DISCARD/ : { *.* }*/
}
