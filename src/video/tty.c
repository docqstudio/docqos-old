#include <core/const.h>
#include <core/math.h>
#include <video/framebuffer.h>
#include <video/tty.h>
#include <task/task.h>
#include <task/signal.h>
#include <task/semaphore.h>
#include <cpu/spinlock.h>
#include <cpu/io.h>
#include <cpu/ringbuffer.h>
#include <memory/kmalloc.h>
#include <memory/buddy.h>
#include <lib/string.h>
#include <filesystem/virtual.h>
#include <filesystem/devfs.h>
#include <time/time.h>

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
   struct {
      unsigned int height;
      unsigned int width;
   } font;
                   /*The font information.*/
   struct {
      unsigned int offsetx;
      unsigned int offsety;
      unsigned int width;
      unsigned int height;
      void *cursor; /*The cursor layer.*/
   } cursor;       /*The tty cursor information.*/
   TTYCharacter new[768 / 18][1024 / 10]; /*The characters.*/
   unsigned char dirty[768 / 18]; /*Is the line dirty?*/
   Semaphore semaphore; /*The semaphore of the tty screen.*/
} TTYScreen;

#define TTY_REFRESH_TICKS         (TIMER_HZ / 1)
#define TTY_REFRESH_LASTEST_TICKS (TIMER_HZ / 3)

#define TIOCSPGRP 5

static TTYScreen ttyMainScreen;
static volatile unsigned int ttyPGRP; 
static RingBuffer ttyKeyboardBuffer;
static RingBuffer ttyReadBuffer;
static KeyboardCallback *ttyKeyboardCallback;

static int ttyIOControl(VFSFile *file,int cmd,UserSpace(void) *data);

static VFSFileOperation ttyOperation = {
   .read = (void *)&ttyRead,
   .write = (void *)&ttyWrite,
   .readDir = 0,
   .lseek = 0,
   .ioctl = &ttyIOControl
};


static int ttyWriteScreen(TTYScreen *screen,const char *data,int length)
{
   char c = 0;
   unsigned int red = 0xff,green = 0xff,blue = 0xff;
   downSemaphore(&screen->semaphore);
   for(int i = 0;i < length;++i)
   {
      switch((c = *data++)) 
      { 
      case '\t':  /*Now ingore it.*/
         c = 8 - (screen->x & 7);
         c = min(c,screen->columns - screen->x);

         for(int i = 0;i < c;++i)
         {
            TTYCharacter *character = &screen->new[screen->y][screen->x++];
            character->character = ' ';
            character->blue = character->red = character->green = 0xff;
         }
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
   screen->new[screen->y][screen->x].character = '\0';
   upSemaphore(&screen->semaphore);
   return 0;
}

static int ttyUpdate(TTYScreen *screen)
{
   static unsigned int lasty = 0;
   unsigned int line = -1;
   downSemaphore(&screen->semaphore);
   frameBufferScroll(screen->scroll);
        /*Scroll the screen.*/

   moveFrameBufferLayer(screen->cursor.cursor,
       screen->font.width * screen->x + screen->cursor.offsetx,
       screen->font.height * screen->fb + screen->cursor.offsety,0,0);
          /*Move the cursor layer.*/
   screen->dirty[screen->y] = 1;
          /*The line of current cursor is dirty.*/
   screen->dirty[lasty] = 1;
          /*The line of previous cursor is dirty,too!*/

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
         {
            frameBufferFillRect(0x00,0x00,0x00,j * screen->font.width,height,
                      screen->width - j * screen->font.width,screen->font.height);
            break; 
         }
         frameBufferDrawChar(0,0,0,character->red,character->green,character->blue, 
              j * screen->font.width,height,screen->new[i][j].character); 
              /*Draw the character to the screen.*/
      }
      screen->dirty[i] = 0; /*Now the line isn't dirty.*/
   }

   if(line != -2 && line != -1 && !screen->scroll)
      frameBufferRefreshLine(line,1);
   else
      frameBufferRefreshLine(0,0); /*Refresh all!*/
   lasty = screen->y;

   screen->scroll = 0;
   upSemaphore(&screen->semaphore);
   return 0;
}

static int ttyKeyboardTask(void *unused)
{
   KeyboardState state = {.ctrl = 0,.shift = 0,.caps = 0,.num = 1,.scroll = 0}; 
   unsigned int data;
   for(;;)
   {
      inRingBuffer(&ttyKeyboardBuffer,&data);
            /*Get the data from the keyboard buffer.*/
      data = (*ttyKeyboardCallback)(data,&state); 
      
      if(!data || data >= 0x80)
         continue;
      if(state.ctrl)
         switch(data)
         {
         case 'c':
         case 'C': /*Ctrl+C SIGINT.*/
            ttyWriteScreen(&ttyMainScreen,"^C",2);
            doKill(ttyPGRP,SIGINT);
            continue;
         case 'd':
         case 'D': /*Ctrl+D EOF.*/
            data = '\0';
            break;
         case '\\': /*Ctrl+\ SIGQUIT.*/
            ttyWriteScreen(&ttyMainScreen,"^\\",2);
            doKill(ttyPGRP,SIGQUIT);
            continue;
         default:
            continue;
         }
      outRingBuffer(&ttyReadBuffer,data);
          /*Write it to ttyReadBuffer for ttyRead.*/
   }
   return 0;
}

static int initTTY(void)
{
   ttyMainScreen.fb = ttyMainScreen.line = ttyMainScreen.scroll = 0;
   ttyMainScreen.x = ttyMainScreen.y = 0;
   ttyMainScreen.width = 1024;
   ttyMainScreen.height = 768;
   ttyMainScreen.lines = 768 / 18;
   ttyMainScreen.columns = 1024 / 10;
   ttyMainScreen.cursor.offsetx = 0;
   ttyMainScreen.cursor.offsety = 0;
   ttyMainScreen.cursor.width = 8;
   ttyMainScreen.cursor.height = 16;
   ttyMainScreen.font.height = 18;
   ttyMainScreen.font.width = 10; /*Init some fields of the screen.*/
   initSemaphore(&ttyMainScreen.semaphore);

   initRingBuffer(&ttyReadBuffer);
   initRingBuffer(&ttyKeyboardBuffer);
   createKernelTask(&ttyKeyboardTask,0); /*Create the task.*/

   ttyMainScreen.cursor.cursor = 
      createFrameBufferLayer(0,0,ttyMainScreen.cursor.width,ttyMainScreen.cursor.height,0);
         /*Create a cursor layer.*/
   return 0;
}

static int registerTTY(void)
{
   return devfsRegisterDevice(&ttyOperation,"tty");
}

int ttyKeyboardPress(KeyboardCallback *callback,u8 data)
{
   outRingBuffer(&ttyKeyboardBuffer,data);
   ttyKeyboardCallback = callback;
   return 0;
}

int ttyWrite(VFSFile *file,UserSpace(const void) *string,u64 size)
{
   PhysicsPage *page = allocPages(0);
   long retval = 0;
   void *buffer;
   if(!page)
      return -ENOMEM;
   
   buffer = getPhysicsPageAddress(page);
   if(size)
      retval = memcpyUser1(buffer,string,size) < 0 ? -EFAULT : size;
   else
      retval = strncpyUser1(buffer,string,4096) - 1;
   if(retval < 0)
      return freePages(page,0),retval;
   ttyWriteScreen(&ttyMainScreen,buffer,retval);
   freePages(page,0);
   return 0;
}

int ttyRead(VFSFile *file,UserSpace(void) *string,u64 data)
{
   unsigned int i = 0;
   unsigned int c;

   setKernelAddressLimit();
   ttyUpdate(&ttyMainScreen);
   switch(data)
   {
   case 1:
      return putUser8Safe(string++,0) == 0 ? 0 : -EFAULT;
   case 0: /*The length of buffer is not long enough.*/
      return data;
   default:
      break;
   }
   for(;;)
   {
      if(inRingBuffer(&ttyReadBuffer,&c) == -EINTR)
         return -EINTR; /*Interrupted by signals.*/
      if(c == '\t')
         continue;
      if(c == '\0' && !i)
         return 0; /*EOF.*/
      else if(c == '\0')
         continue; 
           /*Not the first character,do nothing.*/
      if(c != '\b' || i) /*Back Space.*/
         ttyWriteScreen(&ttyMainScreen,(const char []){c,0},1);
      if(c == '\b' && !i)
         continue; /*It's the first character.*/
      else if(c == '\b')
      {
         ++data;
         --i;
         --string;
         ttyUpdate(&ttyMainScreen);
         continue;
      }
      if(data == 3)
         ttyWriteScreen(&ttyMainScreen,"\n",1);
             /*Line Feed.*/
      if(c != '\n')
         ttyUpdate(&ttyMainScreen);

      setUserAddressLimit();
      if(putUser8Safe(string++,c))
         return -EFAULT;
      --data; /*Write the character to the buffer.*/
      ++i;
      if(data == 2 && c != '\n' && ++i)
         if(putUser8Safe(string++,'\n'))
            return -EFAULT;
      setKernelAddressLimit();

      if(data == 2 || c == '\n')
         break;
   }
   if(putUser8Safe(string++,0))
      return -EFAULT;
   setUserAddressLimit();
   return i;
}

static int ttyIOControl(VFSFile *file,int cmd,UserSpace(void) *data)
{
   switch(cmd)
   {
   case TIOCSPGRP:
      return getUser32Safe(data,(u32 *)&ttyPGRP);
         /*Set the tty PGRP.*/
   default:
      return -EINVAL;
   }

   return 0;
}

subsysInitcall(initTTY);
driverInitcall(registerTTY);
