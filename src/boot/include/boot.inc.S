.set LOADER_SIZE_SECTOR , 0x1 /*The size of loader.*/
.set LOADER_MEMORY_SEG  , 0x7e00 /*The memory that loader will load. (seg)*/
.set LOADER_MEMORY_OFF  , 0x0 /*The memory that loader will load. (off)*/
.set LOADER_MEMORY      , LOADER_MEMORY_SEG * 0x10 + LOADER_MEMORY_OFF
 /*The memory that loader will be load.*/

.ifdef inBoot
LOADER_MAGIC_STRING: .ascii "loader"
                     .byte 0x0 /*The magic string of loader.*/
.endif

.ifdef inLoader
.endif
