#pragma once

int initLocalApic(void);
u8 getLocalApicID(void);

int localApicSendEOI(void);
int setupLocalApicTimer(int disable,u32 time);

