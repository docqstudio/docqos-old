/*The memory of page.*/
/*NOTE:The low 12 bits(0x000) must be 0!!*/
.set PAGE_PML4E_MEMORY , 0x120000
.set PAGE_PDPTE_MEMORY , 0x121000
.set PAGE_PDE_MEMORY   , 0x122000
.set PAGE_PTE_MEMORY   , 0x123000

.set DA_64             , 0x2000
