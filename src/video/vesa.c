#include <video/vesa.h>
#include <core/const.h>
#include <lib/string.h>

#define VBE_INFO_ADDRESS          0x80000
#define VBE_MODE_INFO_ADDRESS     0x90000

#define VGA_BITMAP_FONTS_ADDRESS  0x70000
#define VGA_BITMAP_FONTS_SIZE     0x1000 /*4KB*/

#define FONT_WIDTH_EVERY_CHAR 0x8
#define FONT_HEIGHT_EVERY_CHAR 0x10
/* 8*16 */
#define FONT_BYTES_PER_LINE_EVERY_CHAR (FONT_WIDTH_EVERY_CHAR / 0x8)

#define FONT_DISPLAY_WIDTH (FONT_WIDTH_EVERY_CHAR + 0x2)
#define FONT_DISPLAY_HEIGHT (FONT_HEIGHT_EVERY_CHAR + 0x2)

#define FONT_TAB_WIDTH (FONT_DISPLAY_WIDTH * 0x4)

extern u8 fontASC16[];

static VBEInfo currentVBEInfo = {};
static VBEModeInfo currentVBEModeInfo = {};
static u32 displayPosition = 0;

int initVESA(void)
{
   memcpy((void *) &currentVBEInfo, /*to*/
      (const void *) (VBE_INFO_ADDRESS), /*from*/
      sizeof(VBEInfo));/*n*/
   memcpy((void *) &currentVBEModeInfo,/*to*/
      (const void *) (VBE_MODE_INFO_ADDRESS), /*from*/
      sizeof(VBEModeInfo)); /*n*/
   return 0; /*Successful.*/
}

int fillRect(
   u8 red,u8 green,u8 blue,int x,int y,int width,int height)
{
   u32 color;
   u8 fill1,fill2,fill3;
   const u32 xRes = currentVBEModeInfo.xResolution;
   const u32 yRes = currentVBEModeInfo.yResolution;
   u8 *vram = (u8 *)(pointer)currentVBEModeInfo.physBaseAddr;

   vram += 3*x + 3*xRes*y;

   red >>= 8 - currentVBEModeInfo.redMaskSize;
   green >>= 8 - currentVBEModeInfo.greenMaskSize;
   blue >>= 8 - currentVBEModeInfo.blueMaskSize;

   color = red << currentVBEModeInfo.redFieldPosition;
   color |= green << currentVBEModeInfo.greenFieldPosition;
   color |= blue << currentVBEModeInfo.blueFieldPosition;


   fill1 = (color >> 0) & 0xFF;
   fill2 = (color >> 8) & 0xFF;
   fill3 = (color >> 16) & 0xFF;

   for(int i = 0; i < width; ++i)
   {
      for(int j = 0; j < height; ++j)
      {
         *(vram + i*3 + j*xRes*3 + 0) = fill1;
         *(vram + i*3 + j*xRes*3 + 1) = fill2;
         *(vram + i*3 + j*xRes*3 + 2) = fill3;
      }
   }

   return 0;
}

int drawChar(u8 red,u8 green,u8 blue,int x,int y,unsigned char charDrawing)
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
	    drawPoint(red,green,blue,x + cx,y + cy); /*It's a always-inline function.*/
	                                             /*It defined in vesa.h .*/
      }
   }
   return 0;
}

int drawString(u8 red,u8 green,u8 blue,int x,int y,const char *string)
{
   char c;
   while((c = *(string++)) != 0)
   {
      drawChar(red,green,blue,x,y,c);
      x += FONT_DISPLAY_WIDTH;
   }
   return 0;
}

static int screenUp(int numberOfLine)
{
   if(numberOfLine == 0)
      return 0;
   u8 *vram = (u8 *)(pointer)currentVBEModeInfo.physBaseAddr;
   const u32 xRes = currentVBEModeInfo.xResolution;
   const u32 yRes = currentVBEModeInfo.yResolution;
   const u32 vramSize = xRes*yRes*3;
   const u32 lineClearedSize = numberOfLine*xRes*3*FONT_DISPLAY_HEIGHT;

   memcpy((void *)vram, /*to*/
      (const void *)(vram + lineClearedSize), /*from*/
      vramSize - lineClearedSize);

   fillRect(0x00,0x00,0x00, /*Black.*/
      0x0,yRes - numberOfLine*FONT_DISPLAY_HEIGHT, /*X and Y.*/
      xRes,numberOfLine*FONT_DISPLAY_HEIGHT /*Width and height.*/);

   return 0;
}

int writeColorString(u8 red,u8 green,u8 blue,const char *string)
{
   char c;
   const u32 xRes = currentVBEModeInfo.xResolution;
   const u32 yRes = currentVBEModeInfo.yResolution;
   int x = displayPosition % xRes;
   int y = displayPosition / xRes;

   while((c = *(string++)) != 0)
   {
      switch(c)
      {
      case '\r':
         continue;
         break;
      case '\n': /*Line feed?*/
         x = xRes; /*Let x = the end of the line.*/
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
      default:
         drawChar(red,green,blue,x,y,c);
	 break;
      }
      x += FONT_DISPLAY_WIDTH;
      if(x + FONT_DISPLAY_WIDTH > xRes) /*Line feed?*/
      {
         x = 0;
	 y += FONT_DISPLAY_HEIGHT;
	 if(y + FONT_DISPLAY_HEIGHT > yRes) 
	 {
	    screenUp(1);
	 }
      }
   }

   displayPosition = y * xRes + x; /*Update the next display position.*/
}
