#include <core/const.h>
#include <task/task.h>
#include <task/elf64.h>
#include <task/semaphore.h>
#include <task/signal.h>
#include <task/vkernel.h>
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
extern TaskSignal *taskForkSignal(TaskSignal *old,ForkFlags flags);

extern int taskExitFileSystem(TaskFileSystem *old);
extern int taskExitFiles(TaskFiles *old);
extern int taskExitMemory(TaskMemory *old);
extern int taskExitSignal(TaskSignal *old);

static SpinLock allTaskLock; /*Protect the task list and 'list' field of Task.*/
static SpinLock runnableTaskLock; /*Protect the runnable task list and 'runnable' field of Task.*/
static SpinLock taskFamilyLock; /*Protect 'parent','children' and 'sibling' field of Task.*/

static ListHead allTaskList;
static ListHead runnableTaskList; /*The runnable tasks.*/

static AtomicType pid;

static Task *idleTask;

static int __switchTo(Task *prev,Task *next,u64 rip,u64 rsp) __attribute__((__used__));
   /*If we don't tell gcc that it's used,there will be some error.*/
static int scheduleFirst() __attribute__ ((noreturn));

static Task *allocTask(u64 rip)
{
   PhysicsPage *page = allocAlignedPages(1);
   if(unlikely(!page))
      return 0;
   Task *ret = (Task *)getPhysicsPageAddress(page);
   memset(ret,0,sizeof(Task));
   ret->pid = (atomicAddRet(&pid,1) - 1);
   ret->rsp = 
      (u64)(pointer)(((u8 *)ret) + TASK_KERNEL_STACK_SIZE - 1);
   ret->rip = rip;
   ret->state = TaskUninterruptible; /*The task can't run!!!*/
/* ret->preemption = 0;
   ret->root = ret->pwd = 0;
   for(int i = 0;i < sizeof(ret->fd)/sizeof(ret->fd[0]);++i)
      ret->fd[i] = 0;
   ret->pending = 0;
   ret->blocked = 0;*/ /*They have been set to zero by memset.*/

   initList(&ret->children);
   initList(&ret->sibling);
   initList(&ret->list);
   initList(&ret->runnable);
   return ret;
}

static Task *addTask(Task *ret)
{
   lockSpinLock(&allTaskLock);

   listAddTail(&ret->list,&allTaskList);

   unlockSpinLock(&allTaskLock);

   return ret;
}

static Task *createTask(u64 rip)
{
   Task *ret = allocTask(rip);
   return addTask(ret);
}

static int destoryTask(Task *task)
{
   lockSpinLock(&allTaskLock);

   listDelete(&task->list);
   
   unlockSpinLock(&allTaskLock);
   freePages(getPhysicsPage(task),1);
   return 0;
}

static int __switchTo(Task *prev,Task *next,u64 rip,u64 rsp) /*It will be run by jmp.*/
{
   if(prev)
   {
      prev->rip = rip;
      prev->rsp = rsp;
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

   for(;;)
      asm volatile("hlt");
   return 0;
}

static int scheduleTimeoutCallback(void *arg)
{
   Task *task = (Task *)arg;
   wakeUpTask(task,0); /*Wake up this task and return.*/
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
   if(!from->mm) /*Kernel Task?*/
   {
      if(from->activeMM)
         taskExitMemory(from->activeMM); /*Exit.*/
      from->activeMM = from->mm = 0;
   }
   return 0;
}

int schedule(void)
{
   Task *prev = getCurrentTask();
   Task *next = 0;
   Task *from;

   u64 rflags;
repeat:
   disablePreemption();
   lockSpinLockCloseInterrupt(&runnableTaskLock,&rflags);
   if(prev->state == TaskInterruptible &&
              taskSignalPending(prev))
   {
      prev->state = TaskRunning; /*Restore the status to TaskRunning.*/
      if(!prev->needSchedule) /*Does the task really need to schedule.*/
      {
         unlockSpinLockRestoreInterrupt(&runnableTaskLock,&rflags);
         goto done; /*Do nothing.*/
      }
   }
   prev->needSchedule = 0; /*Clear the need schedule flags.*/

   if(!listEmpty(&runnableTaskList))
      next = listEntry(runnableTaskList.next,Task,runnable);
   if(next) /*Found a runnable task?*/
      listDelete(&next->runnable);
   else if(prev->state == TaskRunning)
      next = prev;
   else
      next = idleTask; /*No runnable tasks,switch to the idle task.*/

   if(next == prev)
   {
      unlockSpinLockRestoreInterrupt(&runnableTaskLock,&rflags);
      goto done;
   }
   if(prev != idleTask && prev->state == TaskRunning && listEmpty(&prev->runnable))
      listAddTail(&prev->runnable,&runnableTaskList);
   unlockSpinLock(&runnableTaskLock); /*Add the previous task to runnable task list.*/

   if(!next->mm) /*Switch to kernel Task?*/
      next->activeMM = taskForkMemory(prev->activeMM,ForkShareMemory);
   else /*Kernel task always uses previous task's mm.*/
      next->activeMM = next->mm; 
   taskSwitchMemory(prev->activeMM,next->activeMM);
   switchTo(prev,next,from);

   finishScheduling(from);
   restoreInterrupt(rflags);
done:
   enablePreemptionNoScheduling();

   if(unlikely(prev->needSchedule))
      goto repeat;
   return 0;
}

int scheduleTimeout(int ms)
{     /*Before call this function,we must set the current task's state to TaskStopping.*/
   Timer timer;
   Task *current = getCurrentTask();
   unsigned long long expire;

   ms /= (MSEC_PER_SEC / TIMER_HZ);
   if(!ms) /*Milliseconds to ticks.*/
      ms = 1;
   initTimer(&timer,&scheduleTimeoutCallback,ms,(void *)current);
             /*Init a timer.*/
   expire = timer.ticks;
   addTimer(&timer);
   
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
   Task *new = createTask((u64)retFromFork);
   if(!new)
      return -ENOMEM;

   new->activeMM = new->mm = 
      taskForkMemory(current->mm,flags);
   if(current->mm && !new->mm)
      goto failed;
   new->fs = taskForkFileSystem(current->fs,flags);
   if(!new->fs)
      goto failed;
   new->files = taskForkFiles(current->files,flags);
   if(!new->files)
      goto failed;
   new->sig = taskForkSignal(current->sig,flags);
   if(current->sig && !new->sig)
      goto failed;
       /*Copy mm,fs and files.*/

   if(regs->rsp == (u64)-1)
      regs->rsp = new->rsp;
   
   new->parent = current;
   lockSpinLock(&taskFamilyLock);
   listAdd(&new->sibling,&new->parent->children);
   unlockSpinLock(&taskFamilyLock);
   
   *(IRQRegisters *)(new->rsp -= sizeof(*regs))
      = *regs; /*They will be popped in retFromFork.*/

   if(flags & ForkWait)
      new->mm->wait = &wait;

   wakeUpTask(new,0); /*Wake up the child task.*/
   if(flags & ForkWait)
   {
      downSemaphore(&wait); /*Wait for exit or execve.*/
      new->mm->wait = 0;
   }

   pagingFlushTLB(); /*Flush the TLBs.*/
   return new->pid;
failed:
   if(new->mm)
      taskExitMemory(new->mm);
   if(new->files)
      taskExitFiles(new->files);
   if(new->fs)
      taskExitFileSystem(new->fs);
   if(new->sig)
      taskExitSignal(new->sig);
   destoryTask(new); /*Destory the new task.*/
   return -ENOMEM;
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

   current->state = TaskZombie; /*Set current to a zombie task.*/
   current->exitCode = n;
   taskExitMemory(current->mm);
   taskExitFileSystem(current->fs);
   taskExitFiles(current->files);
   taskExitSignal(current->sig);
        /*Exit mm,files and files.*/
   current->mm = 0;
   current->fs = 0;
   current->files = 0;
   current->activeMM = 0;
   current->sig = 0;

   lockSpinLock(&taskFamilyLock);
   for(ListHead *list = current->children.next;list != &current->children;
      list = current->children.next)
   {
      Task *child = listEntry(list,Task,sibling);
      listDelete(list);
      listAdd(list,&current->parent->children);
      child->parent = current->parent;
   }
   
   if(current->parent->waiting)
      wakeUpTask(current->parent,0); /*If parent is waiting,wake up.*/
   unlockSpinLock(&taskFamilyLock); 

   schedule();

   asm volatile("ud2");
   for(;;);
}

int doWaitPID(u32 pid,int *result,u8 nowait)
{
   Task *current = getCurrentTask();
   Task *child;
retry:;
   u8 has = 0;
   lockSpinLock(&taskFamilyLock);
   current->state = TaskInterruptible;
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
      case TaskZombie: /*Look for children which are zombie tasks.*/
         current->waiting = 0;    /*Find!!!Restore current->waiting and current->state.*/
         current->state = TaskRunning;
         listDelete(&child->sibling);
         unlockSpinLock(&taskFamilyLock);
         if(result)
            *result = child->exitCode;
         pid = child->pid;
         destoryTask(child); /*Destory the child.*/
         return pid;
      default:
         break;
      }
   }
   if(has && !nowait)
      goto wait;
   current->waiting = 0;
   current->state = TaskRunning;
   unlockSpinLock(&taskFamilyLock);
   return has ? -ETIMEDOUT : -ECHILD;
wait:
   unlockSpinLock(&taskFamilyLock);
   schedule(); /*Wait for a zombie child.*/
                /*Wake up in doExit.*/
   if(taskSignalPending(current))
      return -EINTR;
   goto retry;
}

int doExecve(const char *path,const char *argv[],const char *envp[],IRQRegisters *regs)
{
   int ret = 0;
   u8 arguments[1024];
   int size = 0,i = 0,pos = 0;
   TaskSignal *sig;
   if(!argv) /*Are there any arguments?*/
      goto next;
   for(i = 0;argv[i];++i) /*Copy arguments to the kernel stack.*/
   {
      int len = strlen(argv[i]) + 1; /*The argument's length.*/
                                    /*Add 1 for '\0'.*/
      if(pos + len > sizeof(arguments))
      {    /*If there are too many arguments,just ignore arguments.*/
         size = i = pos = 0;
         goto next;
      }
      memcpy((void *)&arguments[pos],(const void *)argv[i],len);
      argv[i] = (const char *)(pointer)pos; /*Set it to the offset.*/
      pos += len;
   }
   size = (i + 1) * sizeof(void *);
   if(pos + size > sizeof(arguments))
   {    /*Too many?*/
      size = i = pos = 0;
      goto next;
   }
   memcpy((void *)&arguments[pos],(const void *)argv,size);
next:;
   Task *current = getCurrentTask();
   VFSFile *file = openFile(path,O_RDONLY);
   TaskMemory *old = current->mm;
   TaskMemory *new = 0;
   if(isErrorPointer(file))
      return getPointerError(file);
   if(!(file->dentry->inode->mode & S_IXUSR))
      return closeFile(file),-EPERM;
   if(S_ISDIR(file->dentry->inode->mode))
      return closeFile(file),-EISDIR;

   current->activeMM = current->mm =
           taskForkMemory(0,ForkShareNothing);
   new = current->mm;
   ret = -ENOMEM;
   if(!current->mm) /*Failed to alloc */
      goto failed;
   
   taskSwitchMemory(0,current->mm);
   current->mm->exec = file;
   if(pos && i && size) /*There are arguments.*/
      ret = elf64Execve(file,arguments,pos,pos + size,regs);
   else
      ret = elf64Execve(file,0,0,0,regs); /*There aren't arguments.*/
   if(ret)
      goto failed;
   new->vkernel = doMMap(getVKernelFile(),0,DEFAULT_VKERNEL_ADDRESS,0x1000,
              PROT_READ | PROT_EXEC,MAP_FIXED);
   if(isErrorPointer(new->vkernel) && (ret = getPointerError(new->vkernel)))
      goto failed;

   ret = -ENOMEM;
   sig = current->sig;
   
   current->sig = taskForkSignal(0,ForkShareNothing);
   if(!current->sig && ((current->sig = sig) || 1))
      goto failed;
   taskExitSignal(sig);

   for(int j = 0;j < TASK_MAX_FILES;++j)
   {
      VFSFile **file = &current->files->fd[j];
      if(!*file || !((*file)->mode & O_CLOEXEC))
         continue;
      closeFile(*file);
             /*Close the files which have set the O_CLOEXEC mode.*/
      *file = 0;
   }
   if(old)
      taskExitMemory(old);
   return 0;
failed:
   taskSwitchMemory(0,old);
   if(new && ((new->exec = 0) || 1))
      taskExitMemory(new);
   current->mm = old; /*Restore the mm field.*/
   closeFile(file); /*Close the file.*/
   return ret;
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

int wakeUpTask(Task *task,TaskState state)
{
   if(!task)
      return -EINVAL;
   u64 rflags;
   int retval = -EINVAL;
   if(!state)
      state = TaskInterruptible | TaskUninterruptible;
   
   lockSpinLockCloseInterrupt(&runnableTaskLock,&rflags);

   if(!(task->state & state))
      goto out;
   task->state = TaskRunning;
   if(listEmpty(&task->runnable))
      listAdd(&task->runnable,&runnableTaskList);
   retval = 0;

out:
   unlockSpinLockRestoreInterrupt(&runnableTaskLock,&rflags);
   return retval;
}

Task *lookForTaskByPID(unsigned int pid)
{
   Task *ret;
   lockSpinLock(&allTaskLock);

   for(ListHead *tmp = allTaskList.next;tmp != &allTaskList;tmp = tmp->next)
   {
      Task *task = listEntry(tmp,Task,list);
      if(task->pid == pid && (ret = task))
         break;
   } /*It's so slow,we should have a PID Hash Table.*/

   unlockSpinLock(&allTaskLock);
   return ret;
}

int initTask(void)
{
   initList(&allTaskList);
   initList(&runnableTaskList);

   initSpinLock(&allTaskLock);
   initSpinLock(&taskFamilyLock);
   initSpinLock(&runnableTaskLock);

   atomicSet(&pid,0);

   idleTask = createTask((u64)(pointer)(&idle));
   idleTask->state = TaskRunning;
   scheduleFirst(); /*Never return.*/

   for(;;);
}
