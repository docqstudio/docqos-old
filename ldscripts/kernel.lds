OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")  
OUTPUT_ARCH(i386:x86-64)  
ENTRY(_start)  
  
SECTIONS {
   . = 0x8000100000;
   .text : {
      *(.magic)
      *(.text*)
   }

   .rodata : {*(.rodata*)}
   .data : {
      *(COMMON)
      *(.data)
   }
   .bss : {
      *(.bss)
   }

   endAddressOfKernel = .;

   /DISCARD/ : { *.* }
}
