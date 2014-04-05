#include <core/const.h>
#include <task/signal.h>
#include <task/task.h>
#include <task/vkernel.h>
#include <memory/kmalloc.h>
#include <memory/paging.h>
#include <lib/string.h>
#include <cpu/io.h>

static unsigned int getPendingSignal(Task *task,SignalAction *action)
{
   unsigned int retval = 0;
   lockSpinLock(&task->sig->lock);
   if(task->pending & (1 << SIGKILL))
      retval = SIGKILL;
   else if(task->pending & (1 << SIGSTOP))
      retval = SIGSTOP; /*We should deal with SIGKILL and SIGSTOP at first.*/
   else if(!taskSignalPending(task))
      ;
   else for(int i = 1;i < 64;++i)
      if((task->pending & (1 << i)) && !(task->blocked & (1 << i)) && (retval = i))
         break;

   task->pending &= ~(1 << retval);
       /*Now the signal is not pending,cancel it!*/
   if(retval)
      *action = task->sig->actions[retval];
   unlockSpinLock(&task->sig->lock);
   return retval;
}

TaskSignal *taskForkSignal(TaskSignal *old,ForkFlags flags)
{
   if(flags & ForkKernel)
      return 0; /*Kernel tasks needn't signals.*/
   if(flags & ForkShareSignal)
      return (old ? 0 : (atomicAdd(&old->ref,1),old));
   TaskSignal *signal = kmalloc(sizeof(*signal));
   if(!signal)
      return 0;
   if(old)
      *signal = *old;
   else
      memset(signal,0,sizeof(*signal));
         /*Clear the signal actions.*/

   initSpinLock(&signal->lock);
   atomicSet(&signal->ref,0);
   return signal;
}

int taskExitSignal(TaskSignal *old)
{
   if(!old)
      return 0;
   if(atomicAdd(&old->ref,-1))
      kfree(old);
   return 0;
}

int doSignalAction(int sig,SignalAction *action,SignalAction *unused)
{
   if((unsigned int)sig >= 64)
      return -EINVAL; /*No such signal.*/
   if(!action)
      return -EINVAL;
   TaskSignal *signal = getCurrentTask()->sig;

   lockSpinLock(&signal->lock);
   signal->actions[sig] = *action; /*Set the action.*/
   unlockSpinLock(&signal->lock);
   return 0;
}

int doKill(unsigned int pid,unsigned int sig)
{
   if(sig >= 64)
      return -EINVAL;
   Task *task = lookForTaskByPID(pid);
   if(!task)
      return -ESRCH; /*No such task.*/
   if(!task->mm)
      return -EPERM; /*Cannot send signals to kernel tasks.*/
   if(!sig)
      return 0; /*Only test,do nothing.*/
   lockSpinLock(&task->sig->lock);
   task->pending |= 1ul << sig; /*The signal is pending.*/
   unlockSpinLock(&task->sig->lock);

   TaskState state = TaskInterruptible;
   if(sig == SIGCONT || sig == SIGKILL)
      state |= TaskStopping;
   wakeUpTask(task,state);
    /*Wake up the task if the task's state is */
    /*   1.TaskInterruptible (for all signals). */
    /*   2.TaskStopping (for SIGCONT/SIGKILL signals). */
   return 0;
}

int handleSignal(IRQRegisters *reg)
{
   Task *current = getCurrentTask();
   SignalAction action = {};
   SignalContent content;
   unsigned int signum;

   while((signum = getPendingSignal(current,&action)))
   {
      switch(signum)
      {
      case SIGKILL:
         doExit(SIGKILL);
      case SIGSTOP:
         current->state = TaskStopping;
         schedule(); /*Wait until the SIGCONT signal.*/
         continue;
      default:
         break;
      }
      if(action.handler == SIG_DFL)
         switch(signum)
         {
         case SIGCHLD:
         case SIGCONT:
         case SIGURG:
         case SIGWINCH:
            continue; /*Do nothing.*/
         default:
            doExit(signum);
         }
      if(action.handler == SIG_IGN)
         continue;

      content.rax = reg->rax;
      content.rbx = reg->rbx;
      content.rcx = reg->rcx;
      content.rdx = reg->rdx;
      content.rsi = reg->rsi;
      content.rdi = reg->rdi;
      content.r8 = reg->r8;
      content.r9 = reg->r9;
      content.r10 = reg->r10;
      content.r11 = reg->r11;
      content.r12 = reg->r12;
      content.r13 = reg->r13;
      content.r14 = reg->r14;
      content.r15 = reg->r15;
      content.rip = reg->rip; 
          /*Copy most of the interrupt registers to the signal content.*/
      content.blocked = current->blocked;
      current->blocked |= action.mask.value[0];
      current->blocked &= ~((1 << SIGKILL) | (1 << SIGSTOP));
        /*Save the blocked signals and or it with action.mask.value[0].*/

      *((typeof(content) *)(reg->rsp -= sizeof(content))) = 
                   content;
                    /*Store the signal content to the user stack.*/
      *((unsigned long *)(reg->rsp -= sizeof(unsigned long))) = 
              (unsigned long)vkernelSignalReturn(current->mm->vkernel);
              /*The return address of the signal handler.*/
      reg->rip = (unsigned long)action.handler;

      reg->rdi = signum; /*It's the argument of the signal handler.*/
   }
   return 0;
}

int doSignalReturn(IRQRegisters *reg)
{
   SignalContent *content = (void *)reg->rsp;
   
   reg->rax = content->rax;
   reg->rbx = content->rbx;
   reg->rcx = content->rcx;
   reg->rdx = content->rdx;
   reg->rsi = content->rsi;
   reg->rdi = content->rdi;
   reg->r8 = content->r8;
   reg->r9 = content->r9;
   reg->r11 = content->r11;
   reg->r12 = content->r12;
   reg->r13 = content->r13;
   reg->r14 = content->r14;
   reg->r15 = content->r15;
   reg->rip = content->rip; 
     /*Restore the registers from the signal content in the user stack.*/

   reg->rsp += sizeof(*content);
      /*Drop the signal contents.*/

   Task *current = getCurrentTask();
   current->blocked = content->blocked;
   current->blocked &= ~((1 << SIGKILL) | (1 << SIGSTOP));
       /*Restore the blocked signals.*/

   return 0;
}
