OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")  
OUTPUT_ARCH(i386:x86-64)  
ENTRY(_start)  
  
SECTIONS {
   . = 0x8000100000;
   .text : {
      *(.magic)
      *(.text*)
   }
   . = ALIGN(8);
   .rodata : {*(.rodata*)}
   . = ALIGN(8);
   .data : {
      *(COMMON)
      *(.data)
   }
   . = ALIGN(8);
   .bss : {
      *(.bss)
   }
   . = ALIGN(8);

   initcallStart = .;

   .init.initcall : {
      *(.init.initcall0)
      *(.init.initcall1)
      *(.init.initcall2)
      *(.init.initcall3)
   }

   initcallEnd   = .;

   endAddressOfKernel = .;
   
   . = ALIGN(8);

/*   .info.debug : { *(.debug*) }

   /DISCARD/ : { *.* }*/
}
