#include <core/const.h>
#include <video/framebuffer.h>
#include <task/semaphore.h>
#include <cpu/io.h>
#include <lib/string.h>
#include <memory/paging.h>
#include <filesystem/virtual.h>
#include <filesystem/devfs.h>
#include <init/multiboot.h>

#define FONT_WIDTH_EVERY_CHAR 0x8
#define FONT_HEIGHT_EVERY_CHAR 0x10
/* 8*16 */
#define FONT_BYTES_PER_LINE_EVERY_CHAR (FONT_WIDTH_EVERY_CHAR / 0x8)

#define FONT_DISPLAY_WIDTH (FONT_WIDTH_EVERY_CHAR + 0x2)
#define FONT_DISPLAY_HEIGHT (FONT_HEIGHT_EVERY_CHAR + 0x2)

#define FONT_TAB_WIDTH (FONT_DISPLAY_WIDTH * 0x4)

extern u8 fontASC16[];

static MultibootTagFrameBuffer *frameBufferTag = 0;

static u32 displayPosition = 0;
static Semaphore frameBufferDisplayLock = {};

int initFrameBuffer(MultibootTagFrameBuffer *fb)
{
   fb->address += PAGE_OFFSET;
   frameBufferTag = fb;
   initSemaphore(&frameBufferDisplayLock);

   return 0; /*Successful.*/
}

int frameBufferFillRect(
   u8 red,u8 green,u8 blue,int x,int y,int width,int height)
{
   u32 color;
   const u32 __width = frameBufferTag->width;
   volatile u8 *vram = (u8 *)frameBufferTag->address;

   red >>= 8 - frameBufferTag->redMaskSize;
   green >>= 8 - frameBufferTag->greenMaskSize;
   blue >>= 8 - frameBufferTag->blueMaskSize;

   color = red << frameBufferTag->redFieldPosition;
   color |= green << frameBufferTag->greenFieldPosition;
   color |= blue << frameBufferTag->blueFieldPosition;

   if(frameBufferTag->bpp == 24)
   {
      u8 fill1,fill2,fill3;
      fill1 = (color >> 0) & 0xff;
      fill2 = (color >> 8) & 0xff;
      fill3 = (color >> 16) & 0xff;

      vram += 3 * x + 3 * __width * y;
   
      for(int i = 0; i < width; ++i)
      {
         for(int j = 0; j < height; ++j)
         {
            *(vram + i * 3 + j * __width * 3 + 0) = fill1;
            *(vram + i * 3 + j * __width * 3 + 1) = fill2;
            *(vram + i * 3 + j * __width * 3 + 2) = fill3;
         }
      }
   }else if(frameBufferTag->bpp == 32)
   {
      vram += 4 * x + 4 * __width * y;

      for(int i = 0;i < width; ++i)
      {
         for(int j = 0;j < height; ++j)
         {
            *(u32 *)(vram + i * 4 + j * __width * 4 + 0) = color;
         }
      }
   }
   return 0;
}

int frameBufferDrawChar(u8 red,u8 green,u8 blue,int x,int y,unsigned char charDrawing)
{
   u8 *font = fontASC16 + 
      ((int)(charDrawing)) * FONT_HEIGHT_EVERY_CHAR * FONT_BYTES_PER_LINE_EVERY_CHAR;
   for(int cy = 0;cy < FONT_HEIGHT_EVERY_CHAR;++cy)
   {
      for(int cx = FONT_WIDTH_EVERY_CHAR - 1;cx >= 0; --cx)
      {
         u8 numberOfFont = cx / 8;
         u8 mask = 1 << (8 - cx % 8 - 1);
         if(font[cy * FONT_BYTES_PER_LINE_EVERY_CHAR + numberOfFont] & mask)
            frameBufferDrawPoint(red,green,blue,x + cx,y + cy); /*It's a always-inline function.*/
                                                     /*It defined in vesa.h .*/
      }
   }
   return 0;
}

int frameBufferDrawString(u8 red,u8 green,u8 blue,int x,int y,const char *string)
{
   char c;
   while((c = *(string++)) != 0)
   {
      frameBufferDrawChar(red,green,blue,x,y,c);
      x += FONT_DISPLAY_WIDTH;
   }
   return 0;
}

static int frameBufferScreenUp(int numberOfLine)
{
   if(numberOfLine == 0)
      return 0;
   volatile u8 *vram = (u8 *)(frameBufferTag->address);
   const u32 width = frameBufferTag->width;
   const u32 height = frameBufferTag->height;
   const u32 vramSize = width * height * (frameBufferTag->bpp / 8);
   const u32 lineClearedSize = numberOfLine * width * (frameBufferTag->bpp / 8) * FONT_DISPLAY_HEIGHT;

   memcpy((void *)vram, /*to*/
      (const void *)(vram + lineClearedSize), /*from*/
      vramSize - lineClearedSize);

   frameBufferFillRect(0x00,0x00,0x00, /*Black.*/
      0x0,height - numberOfLine*FONT_DISPLAY_HEIGHT, /*X and Y.*/
      width,numberOfLine*FONT_DISPLAY_HEIGHT /*Width and height.*/);
   return 0;
}

int frameBufferWriteStringInColor(u8 red,u8 green,u8 blue,const char *string)
{
   char c;
   const u32 width = frameBufferTag->width;
   const u32 height = frameBufferTag->height;
   const u8 sred = red;
   const u8 sblue = blue;
   const u8 sgreen = green;

   downSemaphore(&frameBufferDisplayLock);
   int x = displayPosition % width;
   int y = displayPosition / width;

   while((c = *(string++)) != 0)
   {
      switch(c)
      {
      case '\r':
         continue;
         break;
      case '\n': /*Line feed?*/
         x = width; /*Let x = the end of the line.*/
         break;
      case '\t':
         {
/* |---tab---||---tab---||---tab---|
 * ------------------- (x+a)--------
 * -----------------|-a-|-----------
 * -----------|--p--|---------------
 * ----------------(x)--------------
 * xyzabcedefdakpoew---------------- */
            int p = x % (FONT_TAB_WIDTH);
            int a = FONT_TAB_WIDTH - p; 
            x -= FONT_DISPLAY_WIDTH; /*Offset x += FONT_DISPLAY_WIDTH;*/
            if(!p)break;
            x += a; 
            break;
         }
      case '\xff': /*Extand command.*/
         c = *string++;
         switch(c)
         {
         case 's': /*Set color command.*/
            red = *string++;
            green = *string++;
            blue = *string++;
            break;
         case 'r': /*Restore color command.*/
            red = sred;
            blue = sblue;
            green = sgreen;
            break;
         }
         x -= FONT_DISPLAY_WIDTH;   /*Offset x += FONT_DISPLAY_WIDTH;*/
         break;
      case '\b':
         x -= FONT_DISPLAY_WIDTH;
         if(x < 0)
         {
            y -= FONT_DISPLAY_HEIGHT;
            x = width - width % FONT_DISPLAY_HEIGHT;
         }
         frameBufferFillRect(0x00,0x00,0x00,x,y,FONT_DISPLAY_WIDTH,FONT_DISPLAY_HEIGHT);
         x -= FONT_DISPLAY_WIDTH;
         break;
      default:
         if(c == ' ')
            break;
         frameBufferDrawChar(red,green,blue,x,y,c);
         break;
      }
      x += FONT_DISPLAY_WIDTH;
      if(x + FONT_DISPLAY_WIDTH > width) /*Line feed?*/
      {
         x = 0;
         y += FONT_DISPLAY_HEIGHT;
         if(y + FONT_DISPLAY_HEIGHT > height) 
         {
            frameBufferScreenUp(1);
            y -= FONT_DISPLAY_HEIGHT;
         }
      }
   }

   displayPosition = y * width + x; /*Update the next display position.*/

   upSemaphore(&frameBufferDisplayLock);
   return 0;
}

