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

typedef enum ForkFlags{
   ForkShareNothing = 0,
   ForkShareMemory  = 1,
   ForkShareFiles   = 2,
   ForkShareFileSystem = 4,
   ForkWait         = 8,
   ForkKernel       = 16
} ForkFlags;

typedef enum TaskState{
   TaskRunning,
   TaskStopping,
   TaskZombie
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

   u32 preemption;
   TaskFiles *files;
   TaskFileSystem *fs;
   TaskMemory *mm;
   TaskMemory *activeMM;

   u8 waiting;
   int exitCode;
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
Task *getCurrentTask(void) __attribute__ ((const));

int doExit(int n) __attribute__ ((noreturn));
int doFork(IRQRegisters *reg,ForkFlags flags);
int doExecve(const char *path,const char *argv[],const char *envp[],IRQRegisters *regs);
int doWaitPID(u32 pid,int *result,u8 wait);

int createKernelTask(KernelTask task,void *arg);
int wakeUpTask(Task *task);

int initTask(void) __attribute__ ((noreturn));
/*It will never return!!!It will be called in the end of kmain.*/

inline int enablePreemptionSchedule(void)
{
   Task *current = getCurrentTask();
   if(!current)
      return 0;
   if(!--current->preemption)
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
   if(current->state == TaskRunning)
      schedule();
   return 0;
}
