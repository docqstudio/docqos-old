.set LOADER_SIZE_SECTOR  , 0x1 /*The size of loader.*/
.set LOADER_MEMORY_SEG   , 0x7e0 /*The memory that loader will load. (seg)*/
.set LOADER_MEMORY_OFF   , 0x0 /*The memory that loader will load. (off)*/
.set LOADER_MEMORY       , LOADER_MEMORY_SEG * 0x10 + LOADER_MEMORY_OFF
 /*The memory that loader will be load.*/


.set KERNEL_SIZE_SECTOR  , 0x40 /*The size of kernel.*/
.set KERNEL_MEMORY_SEG   , 0x1000 /*The memory that kernel will load. (seg)*/
.set KERNEL_MEMORY_OFF   , 0x0 /*The memory that kernel will load. (off)*/
.set KERNEL_MEMORY       , KERNEL_MEMORY_SEG * 0x10 + KERNEL_MEMORY_OFF
 /*The memory that kernel will be load.*/

.set KERNEL_REAL_MEMORY   , 0x100000 /*The buf memory of kernel.*/

.set KERNEL_ENTRY_ADDRESS , 0x100000 /*It is 1MB.*/
                                     /*It is the same as KERNEL_REAL_MEMORY.*/
				     /*Because the kernel is a binary file.*/

.set VBE_INFO_MEMORY_SEG           , 0x8000   /*The memory of vbe controller info.(seg)*/
                                              /*Offset = 0x0.*/
.set VBE_MODE_INFO_MEMORY_SEG      , 0x9000   /*The memory of vbe mode information.(seg)*/
                                              /*Offset = 0x0.*/
.set VBE_DEFAULT_MODE     , 0x118    /*The default mode of VBE modes.*/

.set MEMORY_INFO_NUMBER_MEMORY_SEG , 0x7000 /*The memory of the number of memory info.(seg)*/
                                            /*Offset = 0x0*/
.set MEMORY_INFO_MEMORY_SEG        , 0x7001 /*The memory of memory info. (seg)*/
                                            /*Offset = 0x0.*/

.ifdef inBoot
LOADER_MAGIC_STRING: .ascii "LOADER"
                     .byte 0x0 /*The magic string of loader.*/
.endif

.ifdef inLoader
KERNEL_MAGIC_STRING:.ascii "KERNEL"
                    .byte 0x0
.endif
