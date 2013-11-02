.set LOADER_SIZE_SECTOR  , 0x1 /*The size of loader.*/
.set LOADER_MEMORY_SEG   , 0x7e0 /*The memory that loader will load. (seg)*/
.set LOADER_MEMORY_OFF   , 0x0 /*The memory that loader will load. (off)*/
.set LOADER_MEMORY       , LOADER_MEMORY_SEG * 0x10 + LOADER_MEMORY_OFF
 /*The memory that loader will be load.*/


.set KERNEL_SIZE_SECTOR  , 0x3 /*The size of kernel.*/
.set KERNEL_MEMORY_SEG   , 0x1000 /*The memory that kernel will load. (seg)*/
.set KERNEL_MEMORY_OFF   , 0x0 /*The memory that kernel will load. (off)*/
.set KERNEL_MEMORY       , KERNEL_MEMORY_SEG * 0x10 + KERNEL_MEMORY_OFF
 /*The memory that kernel will be load.*/

.set KERNEL_REAL_MEMORY   , 0x100000 /*The buf memory of kernel.*/

.set KERNEL_ENTRY_ADDRESS , 0x100000 /*-Ttext set it!*/ /*It is 1MB + 1KB.*/

.set VBE_INFO_MEMORY      , 0x7c10   /*The memory of vbe controller information or vbe mode information.*/
.set VBE_DEFAULT_MODE     , 0x118

.ifdef inBoot
LOADER_MAGIC_STRING: .ascii "LOADER"
                     .byte 0x0 /*The magic string of loader.*/
.endif

.ifdef inLoader
KERNEL_MAGIC_STRING:.ascii "KERNEL"
                    .byte 0x0
.endif
