#pragma once

#define SELECTOR_NONE        0x00

#define SELECTOR_KERNEL_CODE (SELECTOR_NONE + 0x08)
#define SELECTOR_DATA        (SELECTOR_KERNEL_CODE + 0x08)

#define SELECTOR_TSS         (SELECTOR_DATA + 0x08)

int initGDT(void);

int tssSetStack(u64 stack);
