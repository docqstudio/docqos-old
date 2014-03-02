#include <core/const.h>
#include <interrupt/interrupt.h>
#include <task/task.h>
#include <filesystem/virtual.h>
#include <acpi/power.h>
#include <time/time.h>

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
static u64 systemGetTimeOfDay(IRQRegisters *reg);
static u64 systemGetDents64(IRQRegisters *reg);
static u64 systemChdir(IRQRegisters *reg);
static u64 systemGetCwd(IRQRegisters *reg);
static u64 systemLSeek(IRQRegisters *reg);
static u64 systemDup(IRQRegisters *reg);
static u64 systemDup2(IRQRegisters *reg);

SystemCallHandler systemCallHandlers[] = {
   &systemExecve, /*0*/
   &systemOpen,
   &systemRead,
   &systemWrite,
   &systemClose,
   &systemFork,  /*5*/
   &systemExit,
   &systemWaitPID,
   &systemReboot,
   &systemGetPID,
   &systemGetTimeOfDay, /*10*/
   &systemGetDents64,
   &systemChdir,
   &systemGetCwd,
   &systemLSeek,
   &systemDup, /*15*/
   &systemDup2
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
   return -ENOSYS;
}

static u64 systemGetPID(IRQRegisters *reg)
{
   return getCurrentTask()->pid;
}

static u64 systemGetTimeOfDay(IRQRegisters *reg)
{
   return doGetTimeOfDay((u64 *)reg->rbx,(void *)reg->rcx);
}

static u64 systemGetDents64(IRQRegisters *reg)
{
   return doGetDents64((int)reg->rbx,(void *)reg->rcx,(u64)reg->rdx);
}

static u64 systemChdir(IRQRegisters *reg)
{
   return doChdir((const char *)reg->rbx);
}

static u64 systemGetCwd(IRQRegisters *reg)
{
   return doGetCwd((char *)reg->rbx,(u64)reg->rcx);
}

static u64 systemLSeek(IRQRegisters *reg)
{
   return doLSeek((int)reg->rbx,(s64)reg->rcx,(int)reg->rdx);
}

static u64 systemDup(IRQRegisters *reg)
{
   return doDup((int)reg->rbx);
}

static u64 systemDup2(IRQRegisters *reg)
{
   return doDup2((int)reg->rbx,(int)reg->rcx);
}

int doSystemCall(IRQRegisters *reg)
{
   u64 ret = (u64)-ENOSYS;
   if(reg->rax >= sizeof(systemCallHandlers) / sizeof(systemCallHandlers[0]))
      goto out;
   ret = (*systemCallHandlers[reg->rax])(reg);
out:
   reg->rax = ret;
   return 0;
}
