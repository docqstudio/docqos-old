#include <core/const.h>
#include <task/task.h>
#include <memory/buddy.h>
#include <video/console.h>
#include <cpu/io.h>
#include <cpu/atomic.h>
#include <cpu/gdt.h>
#include <cpu/spinlock.h>
#include <lib/string.h>
#include <interrupt/interrupt.h>

#define switchTo(prev,next) \
   asm volatile( \
      "pushfq\n\t" \
      "pushq %%rbp\n\t" \
      "movabs $1f,%%rdx\n\t" \
      "movq %%rsp,%%rcx\n\t" \
      "jmp __switchTo\n\t" \
      "1:\n\t" \
      "popq %%rbp\n\t" /*We need to restore %ebp.*/ \
                      /*Or this code won't work.*/ \
      "popfq" \
      : \
      : "D" (prev),"S" (next) \
      : "%rax","%rbx","%rcx","%rdx","%r8","%r9","%r10", \
        "%r11","%r12","%r13","%r14","%r15","memory" \
        /*All registers.*/ \
   );

extern void *endAddressOfKernel;

static SpinLock scheduleLock = {};

static ListHead tasks = {};
static ListHead sleepingTasks = {};

static AtomicType pid = {};

static Task *idleTask = 0;

static int __switchTo(Task *prev,Task *next,u64 rip,u64 rsp) __attribute__((__used__));
   /*If we don't tell gcc that it's used,there will be some error.*/
static int scheduleFirst() __attribute__ ((noreturn));

static Task *allocTask(u64 rip)
{
   Task *ret = (Task *)getPhysicsPageAddress(allocPages(1));
   ret->pid = (atomicAddRet(&pid,1) - 1);
   ret->rsp = 
      (u64)(pointer)(((u8 *)ret) + TASK_KERNEL_STACK_SIZE - 1);
   ret->rip = rip;
   ret->preemption = 0; /*Enable preemption.*/
   initSpinLock(&ret->lock);

   initList(&ret->sleepingList);
   initList(&ret->list);
   return ret;
}

static Task *addTask(Task *ret)
{
   u64 rflags;
   lockSpinLockDisableInterrupt(&scheduleLock,&rflags);

   listAddTail(&ret->list,&tasks);
   listAddTail(&ret->sleepingList,&sleepingTasks);

   unlockSpinLockRestoreInterrupt(&scheduleLock,&rflags);

   return ret;
}

static Task *createTask(u64 rip)
{
   Task *ret = allocTask(rip);
   return addTask(ret);
}

static int destoryTask(Task *task)
{  
   /*When we come to the function,we have locked*/
   /*scheduleLock and task->lock.So we shoudn't lock them again.*/
   if(task->state == TaskSleeping)
      listDelete(&task->sleepingList); 
   task->state = TaskExited; 
   listDelete(&task->list);

   freePages(getPhysicsPage(task),1);
   return 0;
}

static int __switchTo(Task *prev,Task *next,u64 rip,u64 rsp) /*It will be run by jmp.*/
{
   if(prev)
   {
      lockSpinLock(&prev->lock);
      if(prev->state == TaskRunning)
      {
         prev->state = TaskSleeping;
         prev->rip = rip;
         prev->rsp = rsp;
         if(prev != idleTask)
         {
            listAddTail(&prev->sleepingList,&sleepingTasks);
         }
         unlockSpinLock(&prev->lock);
      }else if(prev->state == TaskExited)
      {
         destoryTask(prev);
         /*We don't need to unlock prev->lock.*/
      }
   }
   lockSpinLock(&next->lock);
   next->state = TaskRunning;
   unlockSpinLock(&next->lock);  

   asm volatile(
      "movq %%rax,%%rsp\n\t"
      "jmp *%%rcx"
      :
      : "a"(next->rsp),"c"(next->rip)
   );
   return 0;
}

static int scheduleFirst(void)
{
   switchTo(0,idleTask); /*It will nerver return.*/
   for(;;);
}

/*Define a task.*/
#define TEST_TASK(name) \
static int task##name(void)\
{ \
   Task *cur = getCurrentTask(); \
   printk("Hi!I'm Task " #name ".My PID is %d.\n",cur->pid); \
   printk("(Task " #name ")Now I'm ready to exit.\n"); \
   return 0; \
}

TEST_TASK(A);
TEST_TASK(B);
TEST_TASK(C);
TEST_TASK(D);

static int idle(void)
{
   closeInterrupt();

   printk("\n");
   printkInColor(0x00,0xff,0x00
      ,"----------------IDLE Task Started----------------\n");
   Task *current = getCurrentTask();
   printk("IDLE Task PID:%d\n",current->pid);

   /*finishScheduling();*/
   /*Don't need to do this,because the idle task is scheduled by scheduleFirst.*/

   createKernelTask(taskA);
   createKernelTask(taskB);
   createKernelTask(taskC);
   createKernelTask(taskD);

   startInterrupt();

   schedule();
   for(;;);
   return 0;
}

Task *getCurrentTask(void)
{
   u64 ret;
   asm volatile(
      "movq %%rsp,%%rax\n\t"
      "andq $0xffffffffffffe000,%%rax"
      :"=a" (ret)
   );
   if(ret < (u64)(pointer)endAddressOfKernel)
      ret = 0; /*It will happen before we called initTask.*/
   return (Task *)ret;
}

int finishScheduling(void) /*It will be called in retFromFork,schedule etc.*/
{
   Task *cur = getCurrentTask();
   if(cur->preemption == 0) /*First run.*/
      disablePreemption(); /*It will be enabled when we unlock spin lock.*/
   unlockSpinLockEnableInterrupt(&scheduleLock);
   return 0;
}

int schedule(void)
{
   Task *prev = getCurrentTask();
   if(prev->preemption)
      return 0; 
   Task *next = 0;

   u64 rflags;
   lockSpinLockDisableInterrupt(&scheduleLock,&rflags);
   /*If we schedule successfully,it will be unlocked in finishScheduling.*/

   if(!listEmpty(&sleepingTasks))
   {
      next = listEntry(sleepingTasks.next,Task,sleepingList);
      listDelete(&next->sleepingList);
   }else if((prev->state == TaskRunning)){
      unlockSpinLockRestoreInterrupt(&scheduleLock,&rflags);
      return 0;
   }else
      next = idleTask;

   if(next == prev)
   {
      unlockSpinLockRestoreInterrupt(&scheduleLock,&rflags);
      return 0;
   }

   switchTo(prev,next);

   finishScheduling();
   return 0;
}

int doFork(IRQRegisters *regs)
{
   int retFromFork(void);/*Fork.S*/

   regs->rflags |= (1 << 9); /*Start interrupt.*/

   Task *current = getCurrentTask();
   Task *new = allocTask(0);
   u32 pid = new->pid;
   u64 rsp = new->rsp;

   *new = *current; /*Copy.*/

   new->rip = (u64)retFromFork;
 
   new->rsp = rsp;
   regs->rsp = rsp;
   *(IRQRegisters *)(new->rsp -= sizeof(*regs))
      = *regs; /*They will be popped in retFromFork.*/

   new->pid = pid;

   addTask(new);

   return pid;
}

int doExit(int n)
{
   Task *current = getCurrentTask();
   if(current == idleTask)
   {
      printkInColor(0xff,0x00,0x00,
         "IDLE Task Called Exit!!!\n\n");
      for(;;); /*IDLE Task should never be exited.*/
   }
   u64 rflags;
   lockSpinLockDisableInterrupt(&scheduleLock,&rflags);

   lockSpinLock(&current->lock);
   current->state = TaskExited;
      /*This task will really be destoried in __switchTo.*/
   unlockSpinLock(&current->lock);

   unlockSpinLockEnableInterrupt(&scheduleLock);
   /*We don't need to restore rflags.*/
   schedule();

   for(;;);
}

int createKernelTask(KernelTask task)
{
   int kernelTaskHelper(void); /*Fork.S .*/

   IRQRegisters regs;
   
   memset(&regs,0,sizeof(regs));

   regs.cs = SELECTOR_KERNEL_CODE;
   regs.ss = SELECTOR_DATA;
   
   regs.rbx = (u64)(pointer)task;
   regs.rip = (u64)(pointer)kernelTaskHelper;
      /*See also fork.S .*/

   regs.rflags = storeInterrupt();
   doFork(&regs);

   return 0;
}

int initTask(void)
{
   initList(&tasks);
   initList(&sleepingTasks);

   initSpinLock(&scheduleLock);

   atomicSet(&pid,0);

   idleTask = allocTask((u64)(pointer)(&idle));
   scheduleFirst(); /*Never return.*/

   (void)createTask;
   for(;;);
}
