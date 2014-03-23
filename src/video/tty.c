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

typedef struct TTYCharacter
{
   unsigned char character:7;
   unsigned char change:1;
   unsigned char red;
   unsigned char green;
   unsigned char blue; /*Some information of the character.*/
} TTYCharacter;

typedef struct TTYScreen
{
   unsigned int x; /*The x position of the current character.*/
   unsigned int y; /*The y position of the current character.*/
   unsigned int height; /*The height in pixels.*/
   unsigned int width; /*The width in pixels.*/
   unsigned int lines; /*The lines of the screen.*/
   unsigned int columns; /*The columns of the screen.*/
   unsigned int scroll; 
     /*How many lines should we scroll when we do the refreshing.*/
   unsigned int line; /*The current line.*/
   unsigned int fb; /*......*/
   struct {unsigned int height;unsigned int width;} font;
                   /*The font information.*/
   TTYCharacter new[768 / 18][1024 / 10]; /*The characters.*/
   unsigned char dirty[768 / 18]; /*Is the line dirty?*/
} TTYScreen;

#define TTY_REFRESH_TICKS         (TIMER_HZ / 1)
#define TTY_REFRESH_LASTEST_TICKS (TIMER_HZ / 3)

static Task *ttyTask = 0;
static SpinLock ttyTaskQueueLock;
static ListHead ttyTaskQueue;
static TTYScreen ttyMainScreen;

static VFSFileOperation ttyOperation = {
   .read = (void *)&ttyRead,
   .write = (void *)&ttyWrite,
   .readDir = 0,
   .lseek = 0
};


static int ttyWriteScreen(TTYScreen *screen,const char *data,int length)
{
   char c;
   unsigned int red = 0xff,green = 0xff,blue = 0xff;
   for(int i = 0;i < length;++i)
   {
      switch((c = *data++)) 
      { 
      case '\t':  /*Now ingore it.*/
         break; 
      case '\b': /*Back Space.*/
         if(screen->x == 0)
         { /*To the previous line.*/
            if(screen->y == screen->line)
               break;
            if(!screen->y)
               screen->y = screen->lines;
            --screen->y;
            --screen->fb;
            screen->x = screen->columns;
         }
         screen->new[screen->y][--screen->x].character = ' ';
                   /*Set to a space.*/
         screen->new[screen->y][screen->x].change = 1;
         screen->dirty[screen->y] = 1; /*The line is dirty!*/
         break; 
      case '\n': 
         screen->x = screen->columns; 
         break;
      case '\033':
         if(data[0] == '[' && data[3] == ';' && data[6] == 'm')
            if(data[1] == '0' && data[2] == '1' && data[4] == '3')
            {
               switch(data[5])
               {
               case '2': /*Red.*/
                  red = 0xff;
                  green = blue = 0x00;
                  data += 7;
                  length -= 7;
                  break;
               case '4': /*Blue.*/
                  red = green = 0x00;
                  blue = 0xff;
                  data += 7;
                  length -= 7;
                  break;
               default:
                  break;
               }
            }
         break;
      default: 
         if(c > 0x7e || c < 0x20) 
            c = ' '; 
         TTYCharacter *character = &screen->new[screen->y][screen->x++];
         character->character = c;
         character->red = red;
         character->blue = blue;
         character->green = green;
         screen->dirty[screen->y] = 1;
         screen->new[screen->y][screen->x].character = '\0';
                      /*The next character is '\0'.*/
         break; 
      } 
      if(screen->x == screen->columns) 
      { 
         screen->x = 0; 
         ++screen->y; 
         if(screen->y == screen->lines) 
            screen->y = 0; 
         if(screen->y == screen->line) 
         {  /*We must scroll.*/
            ++screen->line; 
            if(screen->line == screen->lines) 
               screen->line = 0; 
            if(screen->scroll < screen->lines) 
               ++screen->scroll; 
         }else 
            ++screen->fb; 
      } 
   }
   if(!screen->new[screen->y][screen->x].change)
      screen->new[screen->y][screen->x].character = '\0';
   else
      screen->new[screen->y][screen->x].change = 0;
   return 0;
}

static int ttyUpdate(TTYScreen *screen)
{
   unsigned int line = -1;
   frameBufferScroll(screen->scroll);
        /*Scroll the screen.*/

   for(int i = 0;i < screen->lines;++i)
   {
      if(!screen->dirty[i])
         continue; /*The line isn't dirty.*/
      unsigned int height = i - screen->y + screen->fb;
      if(height >= screen->lines)
         height -= screen->lines;
      if(line == -1)
         line = height;
      else
         line = -2;
      height *= screen->font.height;
      for(int j = 0;j < screen->columns;++j) 
      { 
         TTYCharacter *character = &screen->new[i][j];
         if(!character->character) 
            break; 
         frameBufferDrawChar(0,0,0,character->red,character->green,character->blue, 
              j * screen->font.width,height,screen->new[i][j].character); 
              /*Draw the character to the screen.*/
      }
      screen->dirty[i] = 0; /*Now the line isn't dirty.*/
   }
   if(line != -2 && line != -1 && !screen->scroll)
      frameBufferRefreshLine(line,1); /*Only refresh one line.*/
   else
      frameBufferRefreshLine(0,0); /*Refresh all!*/
   
   screen->scroll = 0;

   return 0;
}

static int ttyTaskFunction(void *data)
{
   KeyboardState state = {.caps = 0,.num = 1,.scroll = 0,.shift = 0,.ctrl = 0};
   u64 rflags;
   unsigned long long int ticks = 0; 
              /*The ticks when we did the lastest refreshing.*/
   unsigned long long int current = 0;
              /*The ticks now.*/
   int position = 0;
   int offsetx = 0,offsety = 0;
   TTYTaskQueue *reader = 0;
   void *layer;

   TTYTaskQueue *queue;
   ttyTask = getCurrentTask();
   ttyTask->state = TaskStopping;
   schedule(); /*Wait until receive the first request.*/

   ttyMainScreen.fb = ttyMainScreen.line = ttyMainScreen.scroll = 0;
   ttyMainScreen.x = ttyMainScreen.y = 0;
   ttyMainScreen.width = 1024;
   ttyMainScreen.height = 768;
   ttyMainScreen.lines = 768 / 18;
   ttyMainScreen.columns = 1024 / 10;
   ttyMainScreen.font.height = 18;
   ttyMainScreen.font.width = 10; /*Init some fields of the screen.*/
   memset(ttyMainScreen.dirty,0,sizeof(ttyMainScreen.dirty));

   frameBufferFillRect(0x00,0x00,0x00,0,0,1024,768); /*Clear the screen.*/
   layer = createFrameBufferLayer(1024 / 2 - 45 / 2,768 / 2 - 45 / 2,45,45,0);
               
   frameBufferRefreshLine(0,0); /*Refresh all.*/
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
            ticks = 0;
            ttyUpdate(&ttyMainScreen);
            break;
         case 2: /*Write.*/
            ttyWriteScreen(&ttyMainScreen,queue->s,queue->data);
            if(queue->task) /*Is the task waiting?*/
               wakeUpTask(queue->task);
            else
               kfree(queue->s),kfree(queue);
            if(!ticks)
               ticks = getTicks();
            break;
         case 3: /*Keyboard press.*/
            queue->data = (*queue->callback)(queue->data,&state);
            if(!queue->data)
               break;
            switch(queue->data)
            {
            case KEY_UP:
               offsety -= state.shift ? 8 : 1;
               break;
            case KEY_DOWN:
               offsety += state.shift ? 8 : 1;
               break;
            case KEY_LEFT:
               offsetx -= state.shift ? 8 : 1;
               break;
            case KEY_RIGHT:
               offsetx += state.shift ? 8 : 1;
               break;
            default:
               break;
            }
            if(queue->data >= 0x80)
               break;
            if(state.ctrl && !position && 
                  (queue->data == 'd' || queue->data == 'D'))
            { /*Ctrl + D , EOF!*/
               *reader->s = 0;
               reader->data = 0; /*No data!*/
               wakeUpTask(reader->task);
               reader = 0;
               break;
            }
            if(state.ctrl)
               break;
            if(!reader || position != 0 || queue->data != '\b')
               ttyWriteScreen(&ttyMainScreen,(const char []){queue->data,'\0'},1),
                  (queue->data != '\n' || !reader ? ttyUpdate(&ttyMainScreen) : 0);
            if(!reader)
               break;
            if(queue->data == '\b' && position == 0)
               break;
            else if(queue->data == '\b' && ((--position),1))
               break;
            reader->s[position++] = queue->data;
            if(reader->data == (position - 1) || queue->data == '\n')
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
            ttyUpdate(&ttyMainScreen);

         lockSpinLockCloseInterrupt(&ttyTaskQueueLock,&rflags);
      }
      if(!offsetx && !offsety)
         ttyTask->state = TaskStopping;
      unlockSpinLockRestoreInterrupt(&ttyTaskQueueLock,&rflags);
      if(offsetx || offsety)
      {
         int flag;
         moveFrameBufferLayer(layer,offsetx,offsety,1,1);
                     /*Move the layer.*/
         offsetx = offsety = 0;
         lockSpinLockCloseInterrupt(&ttyTaskQueueLock,&rflags);
         if((flag = listEmpty(&ttyTaskQueue)))
            ttyTask->state = TaskStopping; /*No new requests.*/
         unlockSpinLockRestoreInterrupt(&ttyTaskQueueLock,&rflags);
         if(!flag)
            continue;
      }
      
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
   if((file->mode & O_ACCMODE) != O_WRONLY)
      if((file->mode & O_ACCMODE) != O_RDWR)
         return -EBADFD;
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
   queue->data = len;
   
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
   if((file->mode & O_ACCMODE) != O_RDONLY)
      if((file->mode & O_ACCMODE) != O_RDWR)
         return -EBADFD;
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

   if(queue->data <= 0)
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
