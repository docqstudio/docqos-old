/*Descriptor of GDT.*/
.macro GDTDescriptor base,limit,Attr
    .word ((\limit) & 0xFFFF)
    .word ((\base) & 0xFFFF)
    .byte (((\base) >> 16) & 0xFF)
    .word ((((\limit) >> 8) & 0x0F00) | ((\Attr) & 0xF0FF))
    .byte (((\base) >> 24) & 0xFF)
.endm


/*There are some of GDT's attributes.*/
.set DA_32          , 0x4000
.set DA_16          , 0x0000

.set DA_DPL0        , 0x00
.set DA_DPL1        , 0x40
.set DA_DPL2        , 0x60
.set DA_DPL3        , 0x80

.set DA_DR          , 0x90
.set DA_DRW         , 0x92
.set DA_DRWA        , 0x93
.set DA_C           , 0x98
.set DA_CR          , 0x9A
.set DA_CCO         , 0x9C
.set DA_CCOR        , 0x9E

.set DA_LDT         , 0x82
.set DA_TASK_GATE   , 0x85
.set DA_386TSS      , 0x89
.set DA_386CGATE    , 0x8C
.set DA_386IGATE    , 0x8E
.set DA_386TGATE    , 0x8F

.set DA_LIMIT_4K    , 0x8000


/*Some attributes of CPUID and EFLAGS.*/
.set EFLAGS_AC_BIT  , 0x00040000
