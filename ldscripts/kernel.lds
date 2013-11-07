OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")  
OUTPUT_ARCH(i386:x86-64)  
ENTRY(_start)  
  
SECTIONS {
   . = 0x100000;
   .text : {
      *(.magic)
      *(.text*)
   }
   . = ALIGN(8);
   .rodata : {*(.rodata*)}
   . = ALIGN(8);
   .data : {*(.data)}
   . = ALIGN(8);
   .bss : {*(.bss)}

   /DISCARD/ : { *.* }
}
