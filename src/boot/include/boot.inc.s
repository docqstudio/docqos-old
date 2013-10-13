LOADER_SIZE_SECTOR = 0x1 /*The size of loader.*/
LOADER_MEMORY_SEG = 0x7e00 /*The memory that loader will load. (seg)*/
LOADER_MEMORY_OFF = 0x0 /*The memory that loader will load. (off)*/
LOADER_MAGIC_STRING: .ascii "loader"
                     .byte 0x0 /*The magic string of loader.*/
