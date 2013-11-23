/*The memory of page.*/
.set PAGE_START_MEMORY      , 0xB00000

.set KERNEL_TEMP_PDE_DIR    , 0x60000
.set KERNEL_TEMP_PDPTE_DIR  , 0x62000
.set KERNEL_TEMP_PML4E_DIR  , 0x64000 /*It's temporary,it will reset in kernel.*/

.set PAGE_OFFSET            , 0x8000000000

.set DA_64                  , 0x2000
