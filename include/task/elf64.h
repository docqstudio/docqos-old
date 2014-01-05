#pragma once
#include <core/const.h>

typedef struct VFSFile VFSFile;
typedef struct IRQRegisters IRQRegisters;

int elf64Execve(VFSFile *file,const char *argv[],
   const char *envp[],IRQRegisters *regs);
