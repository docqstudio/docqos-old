#include <core/const.h>
#include <video/framebuffer.h>
#include <video/tty.h>
#include <task/task.h>
#include <cpu/spinlock.h>
#include <cpu/io.h>
#include <memory/kmalloc.h>
#include <lib/string.h>
#include <filesystem/virtual.h>
#include <filesystem/devfs.h>
#include <time/time.h>

typedef struct TTYTaskQueue{
   u8 type; /*1:Read from screen;2:Write to screen;3:Data from keyboard.*/
   Task *task;
   
   u8 data;
   union{
      char *s;
      KeyboardCallback *callback;
   };
   ListHead list;
} TTYTaskQueue;

#define TTY_REFRESH_TICKS         (TIMER_HZ / 1)
#define TTY_REFRESH_LASTEST_TICKS (TIMER_HZ / 3)

static Task *ttyTask = 0;
static SpinLock ttyTaskQueueLock;
static ListHead ttyTaskQueue;

static VFSFileOperation ttyOperation = {
   .read = &ttyRead,
   .write = &ttyWrite,
   .readDir = 0,
   .lseek = 0
};

static int ttyTaskFunction(void *data)
{
   u64 rflags;
   unsigned long long int ticks = 0; 
              /*The ticks when we did the lastest refreshing.*/
   unsigned long long int current = 0;
              /*The ticks now.*/
   int position = 0;
   TTYTaskQueue *reader = 0;

   TTYTaskQueue *queue;
   ttyTask = getCurrentTask();
   for(;;)
   {
      lockSpinLockCloseInterrupt(&ttyTaskQueueLock,&rflags);
      while(!listEmpty(&ttyTaskQueue))
      {
         queue = listEntry(ttyTaskQueue.next,TTYTaskQueue,list);
         listDelete(&queue->list); /*Get the queue and delete it.*/
         unlockSpinLockRestoreInterrupt(&ttyTaskQueueLock,&rflags);

         switch(queue->type)
         {
         case 1: /*Read.*/
            if(reader && (queue->data = -EBUSY))
               break;
            reader = queue;
            position = 0;
            frameBufferWriteStringInColor(0xff,0xff,0xff,"",0,1); /*Refresh.*/
            ticks = 0;
            break;
         case 2: /*Write.*/
            queue->data =
               frameBufferWriteStringInColor(
                     0xff,0xff,0xff,queue->s,queue->data,0);
            if(queue->task) /*Is the task waiting?*/
               wakeUpTask(queue->task);
            else
               kfree(queue->s),kfree(queue);
            if(!ticks)
               ticks = getTicks();
            break;
         case 3: /*Keyboard press.*/
            queue->data = (*queue->callback)(queue->data);
            if(!queue->data)
               break;
            if(!reader || position != 0 || queue->data != '\b')
               frameBufferWriteStringInColor(
                  0xff,0xff,0xff,(const char []){queue->data,'\0'},1,
                  (ticks = 0) || 1); /*We must refresh!*/
            if(!reader)
               break;
            if(queue->data == '\b' && position == 0)
               break;
            else if(queue->data == '\b' && ((--position),1))
               break;
            if(queue->data  != '\n')
               reader->s[position++] = queue->data;
            if(reader->data == position || queue->data == '\n')
            {
               reader->s[position] = '\0';
               reader->data = position;
               wakeUpTask(reader->task); /*Wake up the task.*/
               reader = 0;
            }
            kfree(queue);
            break;
         default:
            break;
         }
         current = getTicks();
         if(ticks && (current - ticks >= TTY_REFRESH_TICKS) &&
            ((ticks = 0) || 1))
            frameBufferWriteStringInColor(0xff,0xff,0xff,"",0,1); /*Refresh,too!*/

         lockSpinLockCloseInterrupt(&ttyTaskQueueLock,&rflags);
      }
      ttyTask->state = TaskStopping;
      unlockSpinLockRestoreInterrupt(&ttyTaskQueueLock,&rflags);
      if(!ticks)
         schedule(); /*Wait the queue.*/
      else
         scheduleTimeout((ticks + TTY_REFRESH_TICKS - current) * (MSEC_PER_SEC / TIMER_HZ));
                /*Wait until a request is coming or timeout.*/
   }
   return 0;
}

static int initTTY(void)
{
   initSpinLock(&ttyTaskQueueLock);
   initList(&ttyTaskQueue);
   createKernelTask(&ttyTaskFunction,0);
   return 0;
}

static int registerTTY(void)
{
   return devfsRegisterDevice(&ttyOperation,"tty");
}

int ttyKeyboardPress(KeyboardCallback *callback,u8 data)
{
   u64 rflags;
   TTYTaskQueue *queue = kmalloc(sizeof(*queue));
   if(unlikely(!queue))
      return -ENOMEM;
   queue->task = 0;
   queue->type = 3; /*Key pressed.*/
   queue->data = data;
   queue->callback = callback; 

   lockSpinLockCloseInterrupt(&ttyTaskQueueLock,&rflags);
   listAddTail(&queue->list,&ttyTaskQueue);
   unlockSpinLockRestoreInterrupt(&ttyTaskQueueLock,&rflags);
            /*Tell the tty task and wake it up.*/
   wakeUpTask(ttyTask);
   return 0;
}

int ttyWrite(VFSFile *file,const void *string,u64 size)
{
   u64 rflags;
   u8 len = size ? size : strlen(string);
   if(len == 0)
      return 0;
   char *s = kmalloc(len + 1);
   if(!s)
      return -ENOMEM;
   memcpy(s,string,len + 1); /*Copy it.*/
   TTYTaskQueue *queue = kmalloc(sizeof(*queue));
   if(!queue && (kfree(s),1))
      return -ENOMEM;
   queue->type = 2;
   queue->task = 0;/*No need to wait it.*/
   queue->s = s; /*The queue and the string will be free in the tty task0*/
   queue->data = size;
   
   lockSpinLockCloseInterrupt(&ttyTaskQueueLock,&rflags);
   listAddTail(&queue->list,&ttyTaskQueue);
   unlockSpinLockRestoreInterrupt(&ttyTaskQueueLock,&rflags);
             /*Tell the tty task and wake it up!*/
   wakeUpTask(ttyTask);
   return 0;
}

int ttyRead(VFSFile *file,void *string,u64 data)
{
   u64 rflags;
   switch(data)
   {
   case 1:
      *(u8 *)string = '\0';
   case 0:
      return data;
   default:
      break;
   }
   char *s = kmalloc(data);
   if(!s)
      return -ENOMEM;
   TTYTaskQueue *queue = kmalloc(sizeof(*queue));
   queue->type = 1;
   queue->data = data - 1;
   queue->s = s;
   queue->task = getCurrentTask(); /*We are waiting.*/

   lockSpinLockCloseInterrupt(&ttyTaskQueueLock,&rflags);
   listAddTail(&queue->list,&ttyTaskQueue);
   getCurrentTask()->state = TaskStopping; /*Stop current.*/
   unlockSpinLockRestoreInterrupt(&ttyTaskQueueLock,&rflags);

   wakeUpTask(ttyTask);
   schedule();

   if(queue->data < 0)
      goto out;
   memcpy(string,s,queue->data);
   ((u8 *)string)[queue->data] = '\0'; /*Set end.*/

out:
   data = queue->data;
   kfree(s);
   kfree(queue); /*Free them.*/
   return data;
}

subsysInitcall(initTTY);
driverInitcall(registerTTY);
