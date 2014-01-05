#include <core/const.h>
#include <task/task.h>
#include <task/elf64.h>
#include <memory/buddy.h>
#include <memory/paging.h>
#include <video/console.h>
#include <cpu/io.h>
#include <cpu/atomic.h>
#include <cpu/gdt.h>
#include <cpu/spinlock.h>
#include <lib/string.h>
#include <interrupt/interrupt.h>
#include <time/time.h>
#include <filesystem/virtual.h>

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

static AtomicType pid = {};

static Task *idleTask = 0;

static int __switchTo(Task *prev,Task *next,u64 rip,u64 rsp) __attribute__((__used__));
   /*If we don't tell gcc that it's used,there will be some error.*/
static int scheduleFirst() __attribute__ ((noreturn));

static Task *allocTask(u64 rip)
{
   PhysicsPage *page = allocPages(1);
   if(unlikely(!page))
      return 0;
   Task *ret = (Task *)getPhysicsPageAddress(page);
   memset(ret,0,sizeof(Task));
   ret->pid = (atomicAddRet(&pid,1) - 1);
   ret->rsp = 
      (u64)(pointer)(((u8 *)ret) + TASK_KERNEL_STACK_SIZE - 1);
   ret->rip = rip;
   ret->state = TaskSleeping;
/*   ret->cr3 = 0;
   ret->preemption = 0;
   ret->root = ret->pwd = 0;
   for(int i = 0;i < sizeof(ret->fd)/sizeof(ret->fd[0]);++i)
      ret->fd[i] = 0;*/
   initSpinLock(&ret->lock);

   initList(&ret->list);
   return ret;
}

static Task *addTask(Task *ret)
{
   u64 rflags;
   lockSpinLockDisableInterrupt(&scheduleLock,&rflags);

   listAddTail(&ret->list,&tasks);

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
      if((prev->state == TaskRunning) || (prev->state == TaskSleeping))
      {
         prev->state = TaskSleeping;
         prev->rip = rip;
         prev->rsp = rsp;
         unlockSpinLock(&prev->lock);
      }else if(prev->state == TaskExited)
      {
         destoryTask(prev);
         /*We don't need to unlock prev->lock.*/
      }else if(prev->state == TaskStopping)
      {
         prev->rip = rip;
         prev->rsp = rsp;
         unlockSpinLock(&prev->lock);
      }
   }
   lockSpinLock(&next->lock);
   next->state = TaskRunning;
   unlockSpinLock(&next->lock);  

   tssSetStack((pointer)(((u8 *)next) + TASK_KERNEL_STACK_SIZE - 1));
   asm volatile(
      "cmpq $0,%%rbx\n\t"
      "je 1f\n\t"
      "movq %%cr3,%%rdx\n\t"
      "cmpq %%rbx,%%rdx\n\t"
      "je 1f\n\t"
      "movq %%rbx,%%cr3\n\t"
      "1:\n\t" /*label next*/
      "movq %%rax,%%rsp\n\t"
      "jmp *%%rcx"
      :
      : "a"(next->rsp),"c"(next->rip),"b"(next->cr3)
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
/*static*/ int task##name(void *arg __attribute__ ((unused)))\
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
   int kinit(void);

   closeInterrupt();
   disablePreemption();

   printk("\n");
   printkInColor(0x00,0xff,0x00
      ,"----------------IDLE Task Started----------------\n");
   Task *current = getCurrentTask();
   printk("IDLE Task PID:%d\n",current->pid);

   /*finishScheduling();*/
   /*Don't need to do this,because the idle task is scheduled by scheduleFirst.*/

/*   if(doInitcalls())
      goto end;

   char *buf[20] = {};
   printk("\nTry to open file 'test1.c'.\n");
   int fd = doOpen("test1.c");
   if(fd < 0)
      printkInColor(0xff,0x00,0x00,"Failed!!This file doesn't exist.\n");
   else
      doClose(fd); *Never happen.*

   printk("Try to open file '/dev/dev.inf'.\n");
   fd = doOpen("/dev/dev.inf");
   if(fd >= 0)
   {
      printkInColor(0x00,0xff,0x00,"Successful!Read data from it:\n");
      int size = doRead(fd,buf,sizeof(buf) - 1);
      doClose(fd); *Close it.*
      buf[size] = '\0';
      printkInColor(0x00,0xff,0xff,"%s\n",buf);
   }
   printk("Run 'mount -t devfs devfs /dev'.\n");
   doMount("/dev",lookForFileSystem("devfs"),0);
   printk("Next,check if '/dev/cdrom0' exists:");
   if(openBlockDeviceFile("/dev/cdrom0"))
      printkInColor(0x00,0xff,0x00,"Yes!\n\n");
   else
      printkInColor(0xff,0x00,0x00,"No!\n\n"); */

   createKernelTask((KernelTask)&kinit,0);

   //createKernelTask(&taskA,0);
   //createKernelTask(&taskB,0);
   //createKernelTask(&taskC,0);
   //createKernelTask(&taskD,0);

   enablePreemption();
   startInterrupt();
  
   schedule();
//end:
   for(;;);
   return 0;
}

static int scheduleTimeoutCallback(void *arg)
{
   Task *task = (Task *)arg;
   wakeUpTask(task); /*Wake up this task and return.*/
   return 0;
}

Task *getCurrentTask(void)
{
   pointer ret;
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
   Task *next = 0;
   ListHead *start = (prev == idleTask) ? &tasks : &prev->list;

   u64 rflags;
   lockSpinLockDisableInterrupt(&scheduleLock,&rflags);
   /*If we schedule successfully,it will be unlocked in finishScheduling.*/

   for(ListHead *list = start->next;list != start;list = list->next)
   {
      Task *temp = listEntry(list,Task,list);
      if(temp->state == TaskSleeping)
      {
         next = temp;
         break;
      }
   }

   if(!next)
   {
      if(prev->state == TaskRunning)
         next = prev;
      else
         next = idleTask;
   }

   if(next == prev)
   {
      unlockSpinLockRestoreInterrupt(&scheduleLock,&rflags);
      return 0;
   }
   switchTo(prev,next);

   finishScheduling();
   return 0;
}

int scheduleTimeout(int ms)
{
   Timer timer;
   Task *current = getCurrentTask();
   unsigned long long expire;

   ms /= (MSEC_PER_SEC / TIMER_HZ);
   if(!ms) /*Milliseconds to ticks.*/
      ms = 1;
   initTimer(&timer,&scheduleTimeoutCallback,ms,(void *)current);
             /*Init a timer.*/
   expire = timer.ticks;
   disablePreemption();
           /*When we set current's state and add timer,this task can't be scheduled.*/
   current->state = TaskStopping; /*If it is scheduled,never return.*/
   addTimer(&timer);
   
   enablePreemption();
   schedule();

   removeTimer(&timer);
   expire -= getTicks();
   expire = (expire > 0) ? expire : 0;
   return expire;
}

int doFork(IRQRegisters *regs)
{
   int retFromFork(void);/*Fork.S*/

   regs->rflags |= (1 << 9); /*Start interrupt.*/

   Task *current = getCurrentTask();
   Task *new = allocTask((u64)retFromFork);

   new->root = current->root;
   new->pwd = current->pwd;
   regs->rsp = new->rsp;
   memcpy((void *)new->fd,(const void *)current->fd,sizeof(current->fd));

   *(IRQRegisters *)(new->rsp -= sizeof(*regs))
      = *regs; /*They will be popped in retFromFork.*/

   addTask(new);

   return new->pid;
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

   unlockSpinLockRestoreInterrupt(&scheduleLock,&rflags);
   
   schedule();

   for(;;);
}

int doExecve(const char *path,const char *argv[],const char *envp[],IRQRegisters *regs)
{
   VFSFile *file = openFile(path);
   if(!file)
      return -1;
   elf64Execve(file,argv,envp,regs);
   closeFile(file);
   return 0;
}

int createKernelTask(KernelTask task,void *arg)
{
   int kernelTaskHelper(void); /*Fork.S .*/

   IRQRegisters regs;
   
   memset(&regs,0,sizeof(regs));

   regs.cs = SELECTOR_KERNEL_CODE;
   regs.ss = SELECTOR_KERNEL_DATA;
   
   regs.rbx = (u64)(pointer)task;
   regs.rip = (u64)(pointer)kernelTaskHelper;
      /*See also fork.S .*/
   regs.rdi = (u64)(pointer)arg;

   regs.rflags = storeInterrupt();
   doFork(&regs);

   return 0;
}

int wakeUpTask(Task *task)
{
   u64 rflags;
   lockSpinLockDisableInterrupt(&scheduleLock,&rflags);
   lockSpinLock(&task->lock);

   do{
      if(task->state != TaskStopping)
         break;
      task->state = TaskSleeping;
   }while(0);

   unlockSpinLock(&task->lock);
   unlockSpinLockRestoreInterrupt(&scheduleLock,&rflags);
   return 0;
}

int initTask(void)
{
   initList(&tasks);

   initSpinLock(&scheduleLock);

   atomicSet(&pid,0);

   idleTask = allocTask((u64)(pointer)(&idle));
   scheduleFirst(); /*Never return.*/

   (void)createTask;
   for(;;);
}
