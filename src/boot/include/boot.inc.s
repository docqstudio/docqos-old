# 1 "include/boot.inc.S"
# 1 "<命令行>"
# 1 "include/boot.inc.S"
.set LOADER_SIZE_SECTOR , 0x1
.set LOADER_MEMORY_SEG , 0x7e0
.set LOADER_MEMORY_OFF , 0x0
.set LOADER_MEMORY , LOADER_MEMORY_SEG * 0x10 + LOADER_MEMORY_OFF



.set KERNEL_SIZE_SECTOR , 0x3
.set KERNEL_MEMORY_SEG , 0x1000
.set KERNEL_MEMORY_OFF , 0x0
.set KERNEL_MEMORY , KERNEL_MEMORY_SEG * 0x10 + KERNEL_MEMORY_OFF


.set KERNEL_REAL_MEMORY , 0x100000

.set KERNEL_ENTRY_ADDRESS , 0x100000



.set VBE_INFO_MEMORY_SEG , 0x8000

.set VBE_MODE_INFO_MEMORY_SEG , 0x9000

.set VBE_DEFAULT_MODE , 0x118

.ifdef inBoot
LOADER_MAGIC_STRING: .ascii "LOADER"
                     .byte 0x0
.endif

.ifdef inLoader
KERNEL_MAGIC_STRING:.ascii "KERNEL"
                    .byte 0x0
.endif
