#pragma once
#include <core/const.h>

int initLocalApic(void);
u8 getLocalApicID(void);

int localApicSendEOI(void);
int setupLocalApicTimer(int disable,u32 time);
u32 getLocalApicTimerCounter(void);

