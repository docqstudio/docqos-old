#include <core/const.h>
#include <interrupt/interrupt.h>
#include <task/task.h>
#include <filesystem/virtual.h>
#include <acpi/power.h>

#define REBOOT_MAGIC_CMD    0xacde147525474417ul
#define POWEROFF_MAGIC_CMD  0x1234aeda78965421ul

typedef u64 (*SystemCallHandler)(IRQRegisters *reg);

static u64 systemExecve(IRQRegisters *regs);
static u64 systemOpen(IRQRegisters *reg);
static u64 systemRead(IRQRegisters *reg);
static u64 systemWrite(IRQRegisters *reg);
static u64 systemClose(IRQRegisters *reg);
static u64 systemFork(IRQRegisters *reg);
static u64 systemExit(IRQRegisters *reg);
static u64 systemWaitPID(IRQRegisters *reg);
static u64 systemReboot(IRQRegisters *reg);
static u64 systemGetPID(IRQRegisters *reg);

SystemCallHandler systemCallHandlers[] = {
   systemExecve, /*0*/
   systemOpen,
   systemRead,
   systemWrite,
   systemClose,
   systemFork,  /*5*/
   systemExit,
   systemWaitPID,
   systemReboot,
   systemGetPID
};

static u64 systemOpen(IRQRegisters *reg)
{
   return doOpen((const char *)reg->rbx);
}

static u64 systemRead(IRQRegisters *reg)
{
   return doRead((int)reg->rbx,(void *)reg->rcx,(u64)reg->rdx);
}

static u64 systemWrite(IRQRegisters *reg)
{
   return doWrite((int)reg->rbx,(const void *)reg->rcx,(u64)reg->rdx);
}

static u64 systemClose(IRQRegisters *reg)
{
   return doClose((int)reg->rbx);
}

static u64 systemExecve(IRQRegisters *reg)
{
   return doExecve((const char *)reg->rbx,(const char **)reg->rcx,
                      (const char **)reg->rdx,reg);
}

static u64 systemFork(IRQRegisters *reg)
{
   IRQRegisters __reg = *reg;
   __reg.rax = 0;
   int ret = doFork(&__reg,ForkShareNothing);
   return ret;
}

static u64 systemWaitPID(IRQRegisters *reg)
{
   return doWaitPID((u32)reg->rbx,(int *)reg->rcx,(u8)reg->rdx);
}

static u64 systemExit(IRQRegisters *reg)
{
   return doExit((int)reg->rbx);
}

static u64 systemReboot(IRQRegisters *reg)
{
   switch(reg->rbx)
   {
   case REBOOT_MAGIC_CMD:
      doReboot();
      break;
   case POWEROFF_MAGIC_CMD:
      doPowerOff();
      break;
   default:
      break;
   }
   return -1;
}

static u64 systemGetPID(IRQRegisters *reg)
{
   return getCurrentTask()->pid;
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
