#include <core/const.h>
#include <task/task.h>
#include <task/elf64.h>
#include <task/semaphore.h>
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

#define switchTo(prev,next,from) \
   asm volatile( \
      "pushfq\n\t" \
      "pushq %%rbp\n\t" \
      "movabs $1f,%%rdx\n\t" \
      "movq %%rsp,%%rcx\n\t" \
      "jmp __switchTo\n\t" \
      "1:\n\t" \
      "popq %%rbp\n\t" /*We need to restore %ebp.*/ \
                      /*Or this code won't work.*/ \
      "popfq\n\t" \
      "movq %%rax,%%rax" \
      : "=a"(from) \
      : "D" (prev),"S" (next) \
      : "%rbx","%rcx","%rdx","%r8","%r9","%r10", \
        "%r11","%r12","%r13","%r14","%r15","memory" \
        /*All registers.*/ \
   );

extern void *endAddressOfKernel;

extern int taskSwitchMemory(TaskMemory *old,TaskMemory *new);
extern TaskFileSystem *taskForkFileSystem(TaskFileSystem *old,ForkFlags flags);
extern TaskFiles *taskForkFiles(TaskFiles *old,ForkFlags flags);
extern TaskMemory *taskForkMemory(TaskMemory *old,ForkFlags flags);
extern int taskExitFileSystem(TaskFileSystem *old);
extern int taskExitFiles(TaskFiles *old);
extern int taskExitMemory(TaskMemory *old);

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

   initList(&ret->children);
   initList(&ret->sibling);
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
   u64 rflags;
   lockSpinLockDisableInterrupt(&scheduleLock,&rflags);
   listDelete(&task->sibling);
   listDelete(&task->list);

   unlockSpinLockRestoreInterrupt(&scheduleLock,&rflags);
   freePages(getPhysicsPage(task),1);
   return 0;
}

static int __switchTo(Task *prev,Task *next,u64 rip,u64 rsp) /*It will be run by jmp.*/
{
   if(prev)
   {
      switch(prev->state)
      {
         case TaskRunning:
            prev->state = TaskSleeping;
         case TaskSleeping:
         case TaskStopping:
            prev->rip = rip;
            prev->rsp = rsp;
            break;
         case TaskZombie:
         default:
            break;
      }
   }
   next->state = TaskRunning;

   tssSetStack((pointer)(((u8 *)next) + TASK_KERNEL_STACK_SIZE - 1));
   asm volatile(
      "movq %%rax,%%rsp\n\t"
      "movq %%rdx,%%rax\n\t"
      "jmp *%%rcx"
      :
      : "a"(next->rsp),"c"(next->rip),"d"(prev)
   );
   return 0;
}

static int scheduleFirst(void)
{
   Task *from;
   switchTo(0,idleTask,from); /*It will nerver return.*/
   for(;;);
}

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

   createKernelTask((KernelTask)kinit,0);

   enablePreemption();
   startInterrupt();
  
   schedule();

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

int finishScheduling(Task *from) /*It will be called in retFromFork,schedule etc.*/
{
   Task *cur = getCurrentTask();
   if(cur->preemption == 0) /*First run.*/
      disablePreemption(); /*It will be enabled when we unlock spin lock.*/
   if(!from->mm) /*Kernel Task?*/
   {
      if(from->activeMM)
         taskExitMemory(from->activeMM); /*Exit.*/
      from->activeMM = from->mm = 0;
   }
   unlockSpinLockEnableInterrupt(&scheduleLock);
   return 0;
}

int schedule(void)
{
   Task *prev = getCurrentTask();
   Task *next = 0;
   Task *from;
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

   if(!next->mm) /*Switch to kernel Task?*/
      next->activeMM = taskForkMemory(prev->mm,ForkShareMemory);
   else /*Kernel task always uses previous task's mm.*/
      next->activeMM = next->mm; 
   taskSwitchMemory(prev->activeMM,next->activeMM);
   switchTo(prev,next,from);

   finishScheduling(from);
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

int doFork(IRQRegisters *regs,ForkFlags flags)
{
   int retFromFork(void);/*Fork.S*/

   Semaphore wait;
   initSemaphore(&wait);
   downSemaphore(&wait);
   regs->rflags |= (1 << 9); /*Start interrupt.*/

   Task *current = getCurrentTask();
   Task *new = allocTask((u64)retFromFork);

   if(regs->rsp == (u64)-1)
      regs->rsp = new->rsp;

   new->parent = current;
   listAdd(&new->sibling,&new->parent->children);

   new->activeMM = new->mm = 
      taskForkMemory(current->mm,flags);
   new->fs = taskForkFileSystem(current->fs,flags);
   new->files = taskForkFiles(current->files,flags);
       /*Copy mm,fs and files.*/

   *(IRQRegisters *)(new->rsp -= sizeof(*regs))
      = *regs; /*They will be popped in retFromFork.*/

   if(flags & ForkWait)
      new->mm->wait = &wait;

   addTask(new);
   if(flags & ForkWait)
   {
      downSemaphore(&wait); /*Wait for exit or execve.*/
      new->mm->wait = 0;
   }

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

   current->exitCode = n;
   taskExitMemory(current->mm);
   taskExitFileSystem(current->fs);
   taskExitFiles(current->files);
        /*Exit mm,files and files.*/
   current->mm = 0;
   current->fs = 0;
   current->files = 0;

   current->state = TaskZombie; /*Set current to a zombie task.*/
   if(current->parent->waiting)
      wakeUpTask(current->parent); /*If parent is waiting,wake up.*/
   
   schedule();

   for(;;);
}

int doWaitPID(u32 pid,int *result,u8 nowait)
{
   Task *current = getCurrentTask();
   Task *child;
retry:;
   u8 has = 0;
   current->state = TaskStopping;
   current->waiting = 1; /*Tell the children that their parent is waiting.*/
   for(ListHead *list = current->children.next;list != &current->children;list = list->next)
   {
      child = listEntry(list,Task,sibling);
      if(pid > 0)
         if(child->pid != pid)
            continue;
      has = 1;
      switch(child->state)
      {
      case TaskZombie:
         goto found;  /*Found a zombie child.*/
      default:
         break;
      }
   }
   if(!has)
   {
      current->waiting = 0;
      current->state = TaskRunning;
      return -1;
   }
   if(nowait) /*If we needn't wait,return -1.*/
   {
      current->waiting = 0;
      current->state = TaskRunning;
      return -1;
   }
   schedule(); /*Wait for a zombie child.*/
                /*Wake up in doExit.*/
   goto retry;
found:
   if(result)
      *result = child->exitCode; /*Save exit code to *result.*/
   pid = child->pid;      /*Return the zombie child's pid.*/
   destoryTask(child); /*Destory the zombie child.*/
   current->waiting = 0;     /*The current task is not waiting.*/
   current->state = TaskRunning;      /*Restore the current task to TaskRunning state.*/
   return pid;
}

int doExecve(const char *path,const char *argv[],const char *envp[],IRQRegisters *regs)
{
   VFSFile *file = openFile(path);
   if(!file)
      return -1;
   if(getCurrentTask()->mm)
      taskExitMemory(getCurrentTask()->mm);
   getCurrentTask()->mm = 0;
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

   regs.rsp = (u64)-1;
   regs.rflags = storeInterrupt();
   doFork(&regs,ForkKernel);

   return 0;
}

int wakeUpTask(Task *task)
{
   if(!task)
      return -1;
   do{
      if(task->state != TaskStopping)
         break;
      task->state = TaskSleeping;
   }while(0);
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
