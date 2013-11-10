#pragma once
#include <core/const.h>

int getCPUID(u32 eaxSet,u32 *eax,u32 *ebx,u32 *ecx,u32 *edx);

int displayCPUBrand(void);

int checkIfCPUHasApic(void);
