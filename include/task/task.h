#pragma once
#include <core/const.h>
#include <core/list.h>
#include <cpu/spinlock_types.h>
#include <interrupt/interrupt.h>

#define TASK_KERNEL_STACK_SIZE (4096*2)

typedef enum TaskState{
   TaskRunning,
   TaskSleeping,
   TaskStopping,
   TaskExited
} TaskState;

typedef struct Task{
   TaskState state;
   u32 pid;

   u64 rip;
   u64 rsp;

   SpinLock lock;

   ListHead list;
   ListHead sleepingList;

   u32 preemption;
} Task;

typedef union TaskKernelStack
{
   u8 stack[TASK_KERNEL_STACK_SIZE - 1];
   Task task;
} TaskKernelStack;

typedef int (*KernelTask)(void *arg);

inline int enablePreemptionSchedule(void) __attribute__ ((always_inline));
inline int enablePreemption(void) __attribute__ ((always_inline));
inline int disablePreemption(void) __attribute__ ((always_inline));
inline int preemptionSchedule(void) __attribute__ ((always_inline));

int schedule(void);
int scheduleTimeout(int ms);
   /*Return 0 if timeout.*/
Task *getCurrentTask(void);

int doExit(int n) __attribute__ ((noreturn));
int doFork(IRQRegisters *reg);

int createKernelTask(KernelTask task,void *arg);
int wakeUpTask(Task *task);

int initTask(void) __attribute__ ((noreturn));
/*It will never return!!!It will be called in the end of kmain.*/

inline int enablePreemptionSchedule(void)
{
   if(!--getCurrentTask()->preemption)
      schedule();
   return 0;
}

inline int enablePreemption(void)
{
   Task *current = getCurrentTask();
   if(current)
     --current->preemption;
   return 0;
}

inline int disablePreemption(void)
{
   Task *current = getCurrentTask();
   if(current)
     ++current->preemption;
   return 0;
}

inline int preemptionSchedule(void)
{
   Task *current = getCurrentTask();
   if(current->preemption)
      return 0;
   return schedule();
}
