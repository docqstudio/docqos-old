#pragma once

#define EXP_START_INT 0
#define EXP_END_INT   20
#define EXP_COUNT     (EXP_END_INT - EXP_START_INT)

#define IRQ_START_INT 40
#define IRQ_END_INT   64
#define IRQ_COUNT     (IRQ_END_INT - IRQ_START_INT)

#define LOCAL_TIMER_INT 0xfe
#define SYS_CALL_INT    0xff

int initIDT(void);
