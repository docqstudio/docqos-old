#pragma once

#define SELECTOR_NONE        0x00

#define SELECTOR_KERNEL_CODE 0x08
#define SELECTOR_USER_CODE   0x13
#define SELECTOR_KERNEL_DATA 0x18
#define SELECTOR_USER_DATA   0x23

#define SELECTOR_TSS         0x28

int initGDT(void);

int tssSetStack(u64 stack);
