#include <core/const.h>
#include <video/framebuffer.h>
#include <task/semaphore.h>
#include <cpu/io.h>
#include <lib/string.h>
#include <memory/paging.h>
#include <filesystem/virtual.h>
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

static void *screenOffBuffer = 0; /*A off-screen buffer.*/
static u32 startAddress; 
   /*The start address of the off-screen buffer to refresh video ram.*/

static int frameBufferScreenUp(int numberOfLine)
{
   if(numberOfLine == 0)
      return 0;
   volatile u8 *vram = (u8 *)(frameBufferTag->address);
   const u32 width = frameBufferTag->width;
   const u32 height = frameBufferTag->height;
   const u32 vramSize = width * height * (frameBufferTag->bpp / 8);
   const u32 lineClearedSize = numberOfLine * width * (frameBufferTag->bpp / 8) * FONT_DISPLAY_HEIGHT;

   if(screenOffBuffer)
   {
      startAddress += lineClearedSize;
      while(startAddress >= vramSize)
         startAddress -= vramSize;
   }else{
      memcpy((void *)vram, /*to*/
         (const void *)(vram + lineClearedSize), /*from*/
         vramSize - lineClearedSize);
   }

   frameBufferFillRect(0x00,0x00,0x00, /*Black.*/
      0x0,height - numberOfLine * FONT_DISPLAY_HEIGHT, /*X and Y.*/
      width,numberOfLine * FONT_DISPLAY_HEIGHT /*Width and height.*/);
   return 0;
}

static int frameBufferRefreshSub(u8 *buffer,u8 *vram,u32 width,
                         u32 height,u32 bpp,u32 __width)
{
   for(int i = 0;i < height;++i)
   {
      memcpy((void *)vram,(const void *)buffer,
                width * bpp); /*Copy to the video ram.*/
      vram += __width * bpp;
      buffer += __width * bpp;
   }
   return 0;
}

static int frameBufferRefresh(u32 x,u32 y,u32 x0,u32 y0)
{  /*Refresh the off-screen buffer to the video ram.*/
   static u32 oldStartAddress = 0;
   u8 *buffer = screenOffBuffer;
   u8 *vram = (void *)frameBufferTag->address;
   const u32 bpp = frameBufferTag->bpp / 8; /*Bytes per pixel.*/
   const u32 width = frameBufferTag->width;
   const u32 height = frameBufferTag->height;
   const u32 vramSize = width * height * bpp;

   if(!buffer)
      return 0;
   if(oldStartAddress != startAddress) 
      (x = y = 0),(x0 = width),(y0 = height);
         /*If the start address changed,refresh all.*/
   oldStartAddress = startAddress;
   buffer += startAddress;
   
   buffer += y * width * bpp;
   vram += y * width * bpp; /*The line.*/

   while(buffer - (u8 *)screenOffBuffer >= vramSize)
      buffer -= vramSize; /*Roll to the start of the off-screen buffer.*/

   u32 free = buffer - (u8 *)screenOffBuffer;
   free = vramSize - free;
   free /= width * bpp;  /*The number of the free lines.*/
   buffer += x * bpp;
   vram += x * bpp;

   if(free < y0 - y) /*The free lines are not enough.*/
   {
      frameBufferRefreshSub(buffer,vram,x0 - x,free,bpp,width);
      buffer = screenOffBuffer + x * bpp; /*Roll to the start.*/
      y += free;
      vram += free * width * bpp; /*Add the offset.*/
   }
   frameBufferRefreshSub(buffer,vram,x0 - x,y0 - y,bpp,width);
   return 0;
}

static int frameBufferFillRectSub(u32 color,u32 bpp,u8 *buffer,
                             u32 width,u32 height,u32 __width)
{
   u8 fill0,fill1,fill2;
   u32 *buffer32 = (void *)buffer;
   switch(bpp)
   {
   case 3: /*Three bytes per pixel.*/
      fill0 = color & 0xff;
      fill1 = (color >>= 8) & 0xff;
      fill2 = (color >>= 8) & 0xff;
      for(int i = 0;i < height;++i)
      {
         for(int j = 0;j < width;++j)
         {
            buffer[j * 3 + 0x0] = fill0;
            buffer[j * 3 + 0x1] = fill1;
            buffer[j * 3 + 0x2] = fill2;
         }
         buffer += __width * 3; /*Next line!*/
      }
      break;
   case 4: /*Four bytes per pixel!*/
      for(int i = 0;i < height;++i)
      {
         for(int j = 0;j < width;++j)
         {
            buffer32[j] = color;
         }
         buffer32 += __width; /*Go to the next line.*/
      }
      break;
   default: /*This should never be happened.*/
      break;
   }
   return 0;
}

int initFrameBuffer(MultibootTagFrameBuffer *fb)
{
   extern void *endAddressOfKernel;
   fb->address += PAGE_OFFSET;
   frameBufferTag = fb;
   initSemaphore(&frameBufferDisplayLock);

   if(fb->type != 1) /*RGB Mode.*/
      return -EPROTONOSUPPORT;
   if(fb->bpp != 24 && fb->bpp != 32)
      return -EPROTONOSUPPORT;

   const u32 width = frameBufferTag->width;
   const u32 height = frameBufferTag->height;
   const u32 bpp = frameBufferTag->bpp / 8;
   const u32 size = width * height * bpp;
   screenOffBuffer = endAddressOfKernel;
   endAddressOfKernel += size;

   memset(screenOffBuffer,0,size);
   return 0; /*Successful.*/
}

int frameBufferFillRect(
   u8 red,u8 green,u8 blue,int x,int y,int width,int height)
{
   u32 color;
   const u32 vramSize = frameBufferTag->width * frameBufferTag->height *
                         (frameBufferTag->bpp / 8);
   const u32 __width = frameBufferTag->width;
   const u32 bpp = frameBufferTag->bpp / 8;
   u8 *vram;
   if(screenOffBuffer)
      vram = screenOffBuffer + startAddress;
   else
      vram = (void *)frameBufferTag->address;

   red >>= 8 - frameBufferTag->redMaskSize;
   green >>= 8 - frameBufferTag->greenMaskSize;
   blue >>= 8 - frameBufferTag->blueMaskSize;

   color = red << frameBufferTag->redFieldPosition;
   color |= green << frameBufferTag->greenFieldPosition;
   color |= blue << frameBufferTag->blueFieldPosition;

   vram += y * __width * bpp;

   if(!screenOffBuffer && (vram += x * bpp)) /*The off-screen buffer is not set.*/
      return frameBufferFillRectSub(color,bpp,vram,width,height,__width);

   while(vram - (u8 *)screenOffBuffer >= vramSize)
      vram -= vramSize;

   u32 free = (vramSize - (vram - (u8 *)screenOffBuffer)) / (__width * bpp);
   vram += x * bpp;
   if(free < height) /*The free lines are not enough.*/
   {
      frameBufferFillRectSub(color,bpp,vram,width,free,__width);
      height -= free;
      vram = screenOffBuffer + x * bpp; /*Roll to the start and add the offset.*/
   }
   frameBufferFillRectSub(color,bpp,vram,width,height,__width);
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
                                                     /*It defined in framebuffer.h .*/
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

int frameBufferWriteStringInColor(u8 red,u8 green,u8 blue,const char *string,
                                      u64 size)
{
   char c;
   const u32 width = frameBufferTag->width;
   const u32 height = frameBufferTag->height;
   const u8 sred = red;
   const u8 sblue = blue;
   const u8 sgreen = green;
   const u8 zero = !size;

   downSemaphore(&frameBufferDisplayLock);
   int x,sx = displayPosition % width;
   int y,sy = displayPosition / width;
   x = sx;
   y = sy;

   for(int i = 0;(i < size) || (zero && *string);++i)
   {
      c = *string++;
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
            if(*string >= '0' && *string <= '9')
               red = (*string++ - '0') * 0x10;
            else
               red = (*string++ - 'a' + 0xa) * 0x10;
            if(*string >= '0' && *string <= '9')
               red += (*string++ - '0') * 0x01;
            else
               red += (*string++ - 'a' + 0xa) * 0x01;
            if(*string >= '0' && *string <= '9')
               green = (*string++ - '0') * 0x10;
            else
               green = (*string++ - 'a' + 0xa) * 0x10;
            if(*string >= '0' && *string <= '9')
               green += (*string++ - '0') * 0x01;
            else
               green += (*string++ - 'a' + 0xa) * 0x01;
            if(*string >= '0' && *string <= '9')
               blue = (*string++ - '0') * 0x10;
            else
               blue = (*string++ - 'a' + 0xa) * 0x10;
            if(*string >= '0' && *string <= '9')
               blue += (*string++ - '0') * 0x01;
            else
               blue += (*string++ - 'a' + 0xa) * 0x01;
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
         if(c > 0x7e || c < 0x20)
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
   
   if(sy == y)
      y += FONT_DISPLAY_HEIGHT;
   if(sy > y)
   {
      u32 tmp = sy;
      sy = y;
      y = tmp;
   }
   frameBufferRefresh(0,sy,width,y);

   upSemaphore(&frameBufferDisplayLock);
   return 0;
}

