#include <core/const.h>
#include <core/math.h>
#include <video/framebuffer.h>
#include <video/console.h>
#include <task/semaphore.h>
#include <cpu/io.h>
#include <lib/string.h>
#include <memory/paging.h>
#include <memory/kmalloc.h>
#include <memory/buddy.h>
#include <memory/vmalloc.h>
#include <filesystem/virtual.h>
#include <init/multiboot.h>

typedef struct FrameBufferLayer
{
   unsigned char *buffer;
   unsigned int x;
   unsigned int y;
   unsigned int width;
   unsigned int height;
} FrameBufferLayer;

typedef struct FrameBufferScreen
{
   unsigned char *map;
   unsigned char *vram;
   unsigned int bpp;
   unsigned int width;
   unsigned int height;
   unsigned int redMaskSize;
   unsigned int greenMaskSize;
   unsigned int blueMaskSize;
   unsigned int redFieldPosition;
   unsigned int greenFieldPosition;
   unsigned int blueFieldPosition;

   FrameBufferLayer *layers[256];
   FrameBufferLayer mainLayer;
   Semaphore semaphore;
} FrameBufferScreen;

#define FONT_WIDTH_EVERY_CHAR 0x8
#define FONT_HEIGHT_EVERY_CHAR 0x10
/* 8*16 */
#define FONT_BYTES_PER_LINE_EVERY_CHAR (FONT_WIDTH_EVERY_CHAR / 0x8)

#define FONT_DISPLAY_WIDTH (FONT_WIDTH_EVERY_CHAR + 0x2)
#define FONT_DISPLAY_HEIGHT (FONT_HEIGHT_EVERY_CHAR + 0x2)

#define FONT_TAB_WIDTH (FONT_DISPLAY_WIDTH * 0x4)

extern u8 fontASC16[];

static u32 displayPosition = 0;

static FrameBufferScreen fbMainScreen;

static u32 frameBufferGetColor(u8 red,u8 green,u8 blue)
{
   u32 color;
   red >>= 8 - fbMainScreen.redMaskSize;
   green >>= 8 - fbMainScreen.greenMaskSize;
   blue >>= 8 - fbMainScreen.blueMaskSize;

   color = red << fbMainScreen.redFieldPosition;
   color |= green << fbMainScreen.greenFieldPosition;
   color |= blue << fbMainScreen.blueFieldPosition;
      /*See also the multiboot specifiction or VBE specifiction.*/
   return color;
}

static int frameBufferRefreshMap(unsigned int x0,unsigned int y0,
               unsigned int x1,unsigned int y1)
{
   FrameBufferScreen *screen = &fbMainScreen;
   FrameBufferLayer **player = &screen->layers[0];
   FrameBufferLayer *layer;
   unsigned char *map;
   unsigned int sid = 0,sid4 = 0;
   while((layer = *player++))
   {
      unsigned int vx = layer->x;
      unsigned int vy = layer->y;
      unsigned int vx0 = layer->width + vx;
      unsigned int vy0 = layer->height + vy;
            /*Get the position of the layer.*/

      if(vx < x0)
         vx = x0;
      if(vx0 > x1)
         vx0 = x1;
      if(vx >= vx0)
         continue;
      
      if(vy < y0)
         vy = y0;
      if(vy0 > y1)
         vy0 = y1;
      if(vy >= vy0)
         continue;

      map = (void *)screen->map + vy * screen->width;
                /*Get the map address of the first line which we should refresh.*/
      for(int y = vy;y < vy0;++y)
      {
         unsigned int x = vx;
         while(x & 3)
         {
            map[x] = sid;
            ++x;
         }
         while(vx0 - x >= 4)
         {
            *(unsigned int *)&map[x] = sid4;
            x += 4;
         }
         while(vx0 - x)
         {
            map[x] = sid;
            ++x;
         }
         map += screen->width; /*Next line.*/
      }
      ++sid;
      sid4 += 0x01010101ul;
   }
   return 0;
}

static int frameBufferRefresh(unsigned int x0,unsigned int y0,
               unsigned int x1,unsigned int y1)
{
   FrameBufferScreen *screen = &fbMainScreen;
   unsigned char *map = screen->map + y0 * screen->width;
   unsigned char *vram = screen->vram + 
      y0 * screen->width * (screen->bpp >> 3);

   switch(screen->bpp)
   {
   case 32:
      for(unsigned int y = y0;y < y1;++y)
      {
         for(unsigned int x = x0;x < x1;)
         {
            unsigned int sid = map[x]; /*Get the ID of the layer.*/
            unsigned int sid4 = sid | sid << 8 | sid << 16 | sid << 24;
            FrameBufferLayer *layer = screen->layers[sid];
            unsigned int base = (x - layer->x) + (y - layer->y) * layer->width;
            unsigned long *buffer = (void *)layer->buffer + (base << 2);
            unsigned long *pvram = (void *)vram + (x << 2); /*Video RAM address.*/
            while(*(unsigned int *)&map[x] == sid4)
            { /*We should refresh 4 pixels here.*/
              /*32 bpp,4 pixels == 128 bits == 2 unsigned long values.*/
               *pvram++ = *buffer++;
               *pvram++ = *buffer++;
               x += 4;
               if(x >= x1)
                  goto next32; /*Next line*/
            }
            int j = 0;
#define FILLONE_32() \
   layer = screen->layers[map[x++]]; \
   base = (x - layer->x - 1) + (y - layer->y) * layer->width; \
   buffer = (void *)layer->buffer + (base << 2); \
   ((unsigned int *)pvram)[j++] = *(unsigned int *)buffer;
            FILLONE_32(); /*Refresh ONE pixel.*/
            FILLONE_32();
            FILLONE_32();
            FILLONE_32();
#undef FILLONE_32
next32:;
         }
         map += screen->width;
         vram += screen->width << 2;
      }
      break;
   case 24:
      for(unsigned int y = y0;y < y1;++y)
      {
         for(unsigned int x = x0;x < x1;)
         {
            unsigned int sid = map[x];
            unsigned int sid4 = sid | sid << 8 | sid << 16 | sid << 24;
            FrameBufferLayer *layer = screen->layers[sid];
            unsigned int base = (x - layer->x) + (y - layer->y) * layer->width;
            unsigned int *buffer32 = (void *)layer->buffer + (base << 1) + base;
            unsigned int *vram32 = (void *)vram + (x << 1) + x;

            while(*(unsigned int *)&map[x] == sid4)
            { /*24 bpp,4 pixels == 96 bits == 3 unsigned int values.*/
               *vram32++ = *buffer32++;
               *vram32++ = *buffer32++;
               *vram32++ = *buffer32++;
               x += 4;
               if(x >= x1)
                  goto next24; /*Next line.*/
            }
            unsigned char *vram8 = (void *)vram32;
            unsigned char *buffer8 = (void *)buffer32;

#define GETONE_24() \
   layer = screen->layers[map[x++]]; \
   base = (x - layer->x - 1) + (y - layer->y) * layer->width; \
   base = (base << 1) + base;

            GETONE_24();
            buffer8 = (void *)layer->buffer + base;
            *(unsigned short *)vram8 = *(unsigned short *)buffer8;
            vram8 += 2;buffer8 += 2;
            *vram8++ = *buffer8++; /*Refresh ONE pixel.*/
            
            GETONE_24();
            buffer8 = (void *)layer->buffer + base;
            *vram8++ = *buffer8++;
            *(unsigned short *)vram8 = *(unsigned short *)buffer8;
            vram8 += 2;buffer8 += 2;

            GETONE_24();
            buffer8 = (void *)layer->buffer + base;
            *(unsigned short *)vram8 = *(unsigned short *)buffer8;
            vram8 += 2;buffer8 += 2;
            *vram8++ = *buffer8++;
            
            GETONE_24();
            buffer8 = (void *)layer->buffer + base;
            *vram8++ = *buffer8++;
            *(unsigned short *)vram8 = *(unsigned short *)buffer8;
            vram8 += 2;buffer8 += 2;
#undef GETONE_32
next24:;
         }
         map += screen->width;
         vram += (screen->width << 1) + screen->width;
                /*Change the map and the video ram to next line.*/
      }
      break;
   }
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

static int __frameBufferFillRect(
   u32 color,int x,int y,int width,int height)
{
   const u32 __width = fbMainScreen.width;
   const u32 bpp = fbMainScreen.bpp >> 3;
   u8 *buffer = fbMainScreen.layers[0]->buffer;

   buffer += y * __width * bpp;
   buffer += x * bpp;

   frameBufferFillRectSub(color,bpp,buffer,width,height,__width);
   return 0;
}

static int frameBufferDrawPoint(u32 color,int x,int y)
{
   const u32 width = fbMainScreen.width;
   const u32 bpp = fbMainScreen.bpp >> 3; /* ">> 3" is "/ 8".*/

   u8 *buffer = fbMainScreen.mainLayer.buffer;
   buffer += y * width * bpp;

   buffer += x * bpp;

   switch(bpp)
   {
   case 3:
      buffer[0] = (color >>= 0) & 0xff;
      buffer[1] = (color >>= 8) & 0xff;
      buffer[2] = (color >>= 8) & 0xff;
      break; /*Update the off-screen buffer.*/
   case 4:
      *(u32 *)buffer = color;
      break;
   }
   return 0;
}

int frameBufferFillRect(
   u8 red,u8 green,u8 blue,int x,int y,int width,int height)
{
   return __frameBufferFillRect(frameBufferGetColor(red,green,blue),x,y,width,height);
}

int initFrameBuffer(MultibootTagFrameBuffer *fb)
{
   extern void *endAddressOfKernel;
   fb->address += PAGE_OFFSET;
   initSemaphore(&fbMainScreen.semaphore);

#define COPY(field) \
   fbMainScreen.field = fb->field

   fbMainScreen.vram = (void *)fb->address;
   COPY(bpp);
   COPY(width);
   COPY(height);
   COPY(redMaskSize);
   COPY(greenMaskSize);
   COPY(blueMaskSize);
   COPY(redFieldPosition);
   COPY(greenFieldPosition);
   COPY(blueFieldPosition); /*Copy the fields to fbMainScreen.*/

#undef COPY

   if(fb->type != 1) /*RGB Mode.*/
      return -EPROTONOSUPPORT;
   if(fb->bpp != 24 && fb->bpp != 32)
      return -EPROTONOSUPPORT;

   const u32 width = fbMainScreen.width;
   const u32 height = fbMainScreen.height;
   const u32 bpp = fbMainScreen.bpp / 8;
   const u32 size = width * height * bpp;
   fbMainScreen.mainLayer.buffer = endAddressOfKernel;
   endAddressOfKernel += size;
   fbMainScreen.map = endAddressOfKernel;
   endAddressOfKernel += width * height * sizeof(unsigned char);
        /*We can't allocPages , kmalloc or vmalloc here,so we do this.*/

   fbMainScreen.layers[0] = &fbMainScreen.mainLayer;
   fbMainScreen.layers[1] = 0;
   FrameBufferLayer *layer = &fbMainScreen.mainLayer;
   layer->x = layer->y = 0;
   layer->width = fb->width; /*Init the fields of fbMainScreen and fbMainScreen.mainLayer .*/
   layer->height = fb->height;

   memset(fbMainScreen.map,0,width * height * sizeof(unsigned char)); /*Set to zero.*/
   memset(fbMainScreen.layers[0]->buffer,0,size);
   return 0; /*Successful.*/
}

int frameBufferDrawChar(u8 bred,u8 bgreen,u8 bblue,
    u8 red,u8 green,u8 blue,int x,int y,unsigned char charDrawing)
{
   u32 back = frameBufferGetColor(bred,bgreen,bblue);
   u32 color = frameBufferGetColor(red,green,blue);
   u8 *font = fontASC16 + 
      ((int)(charDrawing)) * FONT_HEIGHT_EVERY_CHAR * FONT_BYTES_PER_LINE_EVERY_CHAR;
   if(charDrawing == ' ')
      return __frameBufferFillRect(back,x,y,FONT_DISPLAY_WIDTH,FONT_DISPLAY_HEIGHT);
   for(int cy = 0;cy < FONT_HEIGHT_EVERY_CHAR;++cy)
   {
      for(int cx = FONT_WIDTH_EVERY_CHAR - 1;cx >= 0; --cx)
      {
         u8 numberOfFont = cx >> 3; /*cx / 8*/
         u8 mask = 1 << (8 - (cx & 7) - 1); /*cx % 8*/
         if(font[cy * FONT_BYTES_PER_LINE_EVERY_CHAR + numberOfFont] & mask)
            frameBufferDrawPoint(color,x + cx,y + cy); /*Draw a point.*/
         else
            frameBufferDrawPoint(back,x + cx,y + cy);
      }
   }
   return 0;
}

int frameBufferDrawString(u8 red,u8 green,u8 blue,int x,int y,const char *string)
{
   char c;
   while((c = *(string++)) != 0)
   {
      frameBufferDrawChar(0,0,0,red,green,blue,x,y,c);
      x += FONT_DISPLAY_WIDTH;
   }
   return 0;
}

int frameBufferScroll(int numberOfLine)
{
   if(numberOfLine == 0)
      return 0;
   if(numberOfLine >= 41)
      return __frameBufferFillRect(0,0,0,1027,768);
   unsigned char *buffer = fbMainScreen.mainLayer.buffer;
   const u32 width = fbMainScreen.width;
   const u32 height = fbMainScreen.height;
   const u32 vramSize = width * height * (fbMainScreen.bpp / 8);
   const u32 lineClearedSize = numberOfLine * width * (fbMainScreen.bpp / 8) * FONT_DISPLAY_HEIGHT;

   memcpy((void *)buffer, /*to*/
      (const void *)(buffer + lineClearedSize), /*from*/
      vramSize - lineClearedSize);

   frameBufferFillRect(0x00,0x00,0x00, /*Black.*/
      0x0,height - numberOfLine * FONT_DISPLAY_HEIGHT, /*X and Y.*/
      width,numberOfLine * FONT_DISPLAY_HEIGHT /*Width and height.*/);
   return 0;
}


int frameBufferWriteStringInColor(u8 red,u8 green,u8 blue,const char *string,
                                      u64 size,u8 refresh)
{
   char c,all = 0;
   const u32 width = fbMainScreen.width;
   const u32 height = fbMainScreen.height;
   const u8 sred = red;
   const u8 sblue = blue;
   const u8 sgreen = green;
   const u8 zero = !size;

   downSemaphore(&fbMainScreen.semaphore);
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
         frameBufferDrawChar(0,0,0,red,green,blue,x,y,c);
         break;
      }
      x += FONT_DISPLAY_WIDTH;
      if(x + FONT_DISPLAY_WIDTH > width) /*Line feed?*/
      {
         all = 1;
         x = 0;
         y += FONT_DISPLAY_HEIGHT;
         if(y + FONT_DISPLAY_HEIGHT > height) 
         {
            frameBufferScroll(1);
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
   if(refresh)
      frameBufferRefresh(0,all ? 0 : sy,width,all ? height : y);

   upSemaphore(&fbMainScreen.semaphore);
   return 0;
}

int frameBufferRefreshLine(unsigned int line,unsigned int vaild)
{
   if(!vaild)
      return frameBufferRefresh(0,0,fbMainScreen.width,fbMainScreen.height);
   return frameBufferRefresh(0,line * FONT_DISPLAY_HEIGHT,fbMainScreen.width,(line + 1) * FONT_DISPLAY_HEIGHT);
}

void *createFrameBufferLayer(unsigned int x,unsigned int y,
                   unsigned int width,unsigned int height,unsigned char refresh)
{
   FrameBufferLayer *layer = kmalloc(sizeof(*layer));
   if(unlikely(!layer))
      return 0; /*Out Of Memory,OOM.*/
   u64 size = width * height * (fbMainScreen.bpp >> 3);
   void *buffer = vmalloc(size);
   if(!buffer)
      return 0;
   memset(buffer,0xff,size);
   layer->x = x;
   layer->y = y;
   layer->width = width;
   layer->height = height;
   layer->buffer = buffer; /*Init the fields of layer.*/
   
   downSemaphore(&fbMainScreen.semaphore);
   FrameBufferLayer **layers = &fbMainScreen.layers[0];
   while(*layers)
      ++layers;
   *layers++ = layer;
   *layers = 0;
   
   frameBufferRefreshMap(x,y,x + width,y + height);
   if(refresh)
      frameBufferRefresh(x,y,x + width,y + height);
   upSemaphore(&fbMainScreen.semaphore);
   return layer;
}

int moveFrameBufferLayer(void *__layer,unsigned int x,
         unsigned int y,unsigned int fromNow,unsigned char refresh)
{ /*Move a layer to another position.*/
   FrameBufferLayer *layer = __layer;
   unsigned int ox = layer->x;
   unsigned int oy = layer->y; /*Save the old values.*/
 
   downSemaphore(&fbMainScreen.semaphore);
   if(fromNow)
      layer->x += x,layer->y += y;
   else
      layer->x = x,layer->y = y; /*Change the values to new values.*/
   
   if(layer->x + layer->width < layer->x)
      goto failed; /*Overflow.*/
   if(layer->y + layer->height < layer->y)
      goto failed;
   if(layer->x + layer->width >= fbMainScreen.width)
      goto failed; /*Uncorrect position.*/
   if(layer->y + layer->height >= fbMainScreen.height)
      goto failed;

   if(layer->x == ox && layer->y == oy)
      return 0; /*No differences between the old values and the new values.*/
   if(layer->x == ox) /*The x position is the same.*/
      frameBufferRefreshMap(ox,min(oy,layer->y),ox + layer->width,max(oy,layer->y) + layer->height);
   else if(layer->y == oy) /*The y postion is the same.*/
      frameBufferRefreshMap(min(ox,layer->x),oy,max(ox,layer->x) + layer->width,oy + layer->height);
   else /*No position is the same.*/
      frameBufferRefreshMap(layer->x,layer->y,layer->x + layer->width,layer->y + layer->height),
      frameBufferRefreshMap(ox,oy,layer->width + ox,layer->height + oy);
   if(!refresh)
      return 0;
   if(layer->x == ox) /*Refresh them to the video ram.*/
      frameBufferRefresh(ox,min(oy,layer->y),ox + layer->width,max(oy,layer->y) + layer->height);
   else if(layer->y == oy)
      frameBufferRefresh(min(ox,layer->x),oy,max(ox,layer->x) + layer->width,oy + layer->height);
   else
      frameBufferRefresh(layer->x,layer->y,layer->x + layer->width,layer->y + layer->height),
      frameBufferRefresh(ox,oy,layer->width + ox,layer->height + oy);
   upSemaphore(&fbMainScreen.semaphore);
   return 0; /*Successful!*/
failed:
   layer->x = ox;
   layer->y = oy;
   upSemaphore(&fbMainScreen.semaphore);
   return -EINVAL;
}

