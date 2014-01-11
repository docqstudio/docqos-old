#include <core/const.h>
#include <interrupt/interrupt.h>
#include <task/task.h>
#include <filesystem/virtual.h>

typedef int (*SystemCallHandler)(IRQRegisters *reg);

static int systemExecve(IRQRegisters *regs);
static int systemOpen(IRQRegisters *reg);
static int systemRead(IRQRegisters *reg);
static int systemWrite(IRQRegisters *reg);
static int systemClose(IRQRegisters *reg);

SystemCallHandler systemCallHandlers[] = {
   systemExecve,
   systemOpen,
   systemRead,
   systemWrite,
   systemClose
};

static int systemOpen(IRQRegisters *reg)
{
   return doOpen((const char *)reg->rbx);
}

static int systemRead(IRQRegisters *reg)
{
   return doRead((int)reg->rbx,(void *)reg->rcx,(u64)reg->rdx);
}

static int systemWrite(IRQRegisters *reg)
{
   return doWrite((int)reg->rbx,(const void *)reg->rcx,(u64)reg->rdx);
}

static int systemClose(IRQRegisters *reg)
{
   return doClose((int)reg->rbx);
}

static int systemExecve(IRQRegisters *reg)
{
   return doExecve((const char *)reg->rbx,0,0,reg);
}

int doSystemCall(IRQRegisters *reg)
{
   u64 ret = (u64)-1;
   if(reg->rax >= sizeof(systemCallHandlers) / sizeof(systemCallHandlers[0]))
      goto out;
   ret = (*systemCallHandlers[reg->rax])(reg);
out:
   reg->rax = ret;
   return 0;
}
