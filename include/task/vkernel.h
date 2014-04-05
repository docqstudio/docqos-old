#pragma once
#include <core/const.h>

#define DEFAULT_VKERNEL_ADDRESS   0xfffff000

typedef struct VFSFile VFSFile;

VFSFile *getVKernelFile(void);
void *vkernelSignalReturn(void *base);
void *vkernelSyscallEntry(void *base);
