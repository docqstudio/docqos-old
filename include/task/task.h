#pragma once
#include <core/const.h>
#include <core/list.h>
#include <cpu/spinlock_types.h>
#include <cpu/atomic.h>
#include <interrupt/interrupt.h>

#define TASK_KERNEL_STACK_SIZE (4096*2)
#define TASK_MAX_FILES         20

typedef struct VFSDentry VFSDentry;
typedef struct VFSFile VFSFile;
typedef struct Semaphore Semaphore;
typedef struct Task Task;
typedef struct VirtualMemoryArea VirtualMemoryArea;
typedef struct TaskFileSystem TaskFileSystem;
typedef struct TaskMemory TaskMemory;
typedef struct TaskFiles TaskFiles;
typedef struct TaskSignal TaskSignal;

typedef enum ForkFlags{
   ForkShareNothing = 0,
   ForkShareMemory  = 1,
   ForkShareFiles   = 2,
   ForkShareFileSystem = 4,
   ForkShareSignal  = 8,
   ForkWait         = 16,
   ForkKernel       = 32
} ForkFlags;

typedef enum TaskState{
   TaskRunning          = 1,
   TaskInterruptible    = 2,
   TaskUninterruptible  = 4,
   TaskZombie           = 8,
   TaskStopping         = 16
} TaskState;

typedef struct Task{
   TaskState state;
   Task *parent;
   u32 pid;

   u64 rip;
   u64 rsp;

   ListHead list;
   ListHead children;
   ListHead sibling;
   ListHead runnable;

   u32 preemption;
   TaskFiles *files;
   TaskFileSystem *fs;
   TaskMemory *mm;
   TaskMemory *activeMM;
   TaskSignal *sig;
   u64 pending;
   u64 blocked;

   u8 waiting;
   u8 needSchedule;
   int exitCode;
} Task;

typedef union TaskKernelStack
{
   u8 stack[TASK_KERNEL_STACK_SIZE - 1];
   Task task;
} TaskKernelStack;

typedef int (*KernelTask)(void *arg);

inline int enablePreemptionNoScheduling(void) __attribute__ ((always_inline));
inline int enablePreemption(void) __attribute__ ((always_inline));
inline int disablePreemption(void) __attribute__ ((always_inline));
inline int preemptionSchedule(void) __attribute__ ((always_inline));

int schedule(void);
int scheduleTimeout(int ms);
   /*Return 0 if timeout.*/
Task *getCurrentTask(void) __attribute__ ((const));

int doExit(int n) __attribute__ ((noreturn));
int doFork(IRQRegisters *reg,ForkFlags flags);
int doExecve(const char *path,const char *argv[],const char *envp[],IRQRegisters *regs);
int doWaitPID(u32 pid,int *result,u8 wait);

int createKernelTask(KernelTask task,void *arg);
int wakeUpTask(Task *task,TaskState state);
Task *lookForTaskByPID(unsigned int pid);

int initTask(void) __attribute__ ((noreturn));
/*It will never return!!!It will be called in the end of kmain.*/

inline int enablePreemptionNoScheduling(void)
{
   Task *current = getCurrentTask();
   if(!current)
      return 0;
   --current->preemption;
   return 0;
}

inline int enablePreemption(void)
{
   Task *current = getCurrentTask();
   if(current)
     if(!--current->preemption && current->state == TaskRunning)
        if(current->needSchedule)
           schedule();
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
   if(current->state == TaskRunning)
      schedule();
   return 0;
}
