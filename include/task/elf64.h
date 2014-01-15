#pragma once
#include <core/const.h>

typedef struct VFSFile VFSFile;
typedef struct IRQRegisters IRQRegisters;

int elf64Execve(VFSFile *file,u8 *arguments,u64 pos,u64 end,IRQRegisters *regs);
