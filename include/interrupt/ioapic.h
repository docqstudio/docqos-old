#pragma once

int initIOApic(void);

int ioApicDisableIRQ(u8 irq);
int ioApicEnableIRQ(u8 irq);
