#include <core/const.h>
#include <video/framebuffer.h>
#include <video/tty.h>
#include <task/task.h>
#include <cpu/spinlock.h>
#include <memory/kmalloc.h>
#include <lib/string.h>
#include <filesystem/virtual.h>
#include <filesystem/devfs.h>

typedef u8 (KeyboardCallback)(u8 data);

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
   int position = 0;
   TTYTaskQueue *reader = 0;

   TTYTaskQueue *queue;
   ttyTask = getCurrentTask();
   for(;;)
   {
      lockSpinLock(&ttyTaskQueueLock);
      while(!listEmpty(&ttyTaskQueue))
      {
         queue = listEntry(ttyTaskQueue.next,TTYTaskQueue,list);
         listDelete(&queue->list); /*Get the queue and delete it.*/
         unlockSpinLock(&ttyTaskQueueLock);

         switch(queue->type)
         {
         case 1:
            if(reader && (queue->data = -EBUSY))
               break;
            reader = queue;
            position = 0;
            break;
         case 2:
            queue->data =
               frameBufferWriteString(queue->s);
            if(queue->task) /*Is the task waiting?*/
               wakeUpTask(queue->task);
            else
               kfree(queue->s),kfree(queue);
            break;
         case 3:
            queue->data = (*queue->callback)(queue->data);
            if(!queue->data)
               break;
            if(!reader || position != 0 || queue->data != '\b')
               frameBufferWriteString((const char []){queue->data,'\0'});
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

         lockSpinLock(&ttyTaskQueueLock);
      }
      ttyTask->state = TaskStopping;
      unlockSpinLock(&ttyTaskQueueLock);
      schedule(); /*Wait the queue.*/
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
   TTYTaskQueue *queue = kmalloc(sizeof(*queue));
   if(unlikely(!queue))
      return -ENOMEM;
   queue->task = 0;
   queue->type = 3; /*Key pressed.*/
   queue->data = data;
   queue->callback = callback; 
   lockSpinLock(&ttyTaskQueueLock);
   listAddTail(&queue->list,&ttyTaskQueue);
   unlockSpinLock(&ttyTaskQueueLock);
            /*Tell the tty task and wake it up.*/
   wakeUpTask(ttyTask);
   return 0;
}

int ttyWrite(VFSFile *file,const void *string,u64 size)
{
   u8 len = strlen(string);
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
   queue->data = 0;
   
   lockSpinLock(&ttyTaskQueueLock);
   listAddTail(&queue->list,&ttyTaskQueue);
   unlockSpinLock(&ttyTaskQueueLock);
             /*Tell the tty task and wake it up!*/
   wakeUpTask(ttyTask);
   return 0;
}

int ttyRead(VFSFile *file,void *string,u64 data)
{
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

   lockSpinLock(&ttyTaskQueueLock);
   listAddTail(&queue->list,&ttyTaskQueue);
   getCurrentTask()->state = TaskStopping; /*Stop current.*/
   unlockSpinLock(&ttyTaskQueueLock);

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
