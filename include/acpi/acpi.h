#pragma once
#include <core/const.h>

int initACPI(void);

u8 *getIOApicAddress(void);
u8 *getLocalApicAddress(void);
