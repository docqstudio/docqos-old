#include <video/vesa.h>
#include <core/const.h>
#include <lib/string.h>
#include <memory/paging.h>

typedef struct VBEInfo{
   u8 signature[4];
   u16 version;

   u32 oemStringPtr;
   u32 capabilities;
   u32 videoModePtr;
   u16 totalMemory;

   u16 oemSoftwareRev;
   u32 oemVendorNamePtr;
   u32 oemProductNamePtr;
   u32 oemProductRevPtr;

   u8 reserved[222];

   u8 oemData[256];
} __attribute__ ((packed)) VBEInfo;

typedef struct VBEModeInfo{
   u16 modeAttributes;              /*Offset 0x00.*/
   u8 winAAttributes;               /*Offset 0x02.*/
   u8 winBAttributes;               /*Offset 0x03.*/
   u16 winGranularity;              /*Offset 0x04.*/
   u16 winSize;                     /*Offset 0x06.*/
   u16 winASegment;                 /*Offset 0x08.*/
   u16 winBSegment;                 /*Offset 0x0A.*/
   u32 winFuncPtr;                  /*Offset 0x0C.*/
   u16 bytesPerScanLine;            /*Offset 0x10.*/

   /*VBE 1.2.*/
   u16 xResolution;                 /*Offset 0x12.*/
   u16 yResolution;                 /*Offset 0x14.*/
   u8 xCharSize;                    /*Offset 0x16.*/
   u8 yCharSize;                    /*Offset 0x17.*/
   u8 numberOfPlanes;               /*Offset 0x18.*/
   u8 bitsPerPixel;                 /*Offset 0x19.*/
   u8 numberOfBanks;                /*Offset 0x1A.*/
   u8 memoryModel;                  /*Offset 0x1B.*/
   u8 bankSize;                     /*Offset 0x1C.*/
   u8 numberOfImagePages;           /*Offset 0x1D.*/
   u8 reserved1;                    /*Offset 0x1E.*/
   u8 redMaskSize;                  /*Offset 0x1F.*/
   u8 redFieldPosition;             /*Offset 0x20.*/
   u8 greenMaskSize;                /*Offset 0x21.*/
   u8 greenFieldPosition;           /*Offset 0x22.*/
   u8 blueMaskSize;                 /*Offset 0x23.*/
   u8 blueFieldPosition;            /*Offset 0x24.*/
   u8 rsvdMaskSize;                 /*Offset 0x25.*/
   u8 rsvdFieldPosition;            /*Offset 0x26.*/
   u8 directColorModeInfo;          /*Offset 0x27.*/

   /*VBE 2.0.*/
   u32 physBaseAddr;                /*Offset 0x28.*/
   u32 reserved2;                   /*Offset 0x2C.*/
   u16 reserved3;                   /*Offset 0x30.*/

   /*VBE 3.0.*/
   u16 linBytesPerScanLine;         /*Offset 0x32.*/
   u8 bnkNumberOfImagePages;        /*Offset 0x34.*/
   u8 linNumberOfImagePages;        /*Offset 0x35.*/
   u8 linRedMaskSize;               /*Offset 0x36.*/
   u8 linRedFieldPosition;          /*Offset 0x37.*/
   u8 linGreenMaskSize;             /*Offset 0x38.*/
   u8 linGreenFieldPosition;        /*Offset 0x39.*/
   u8 linBlueMaskSize;              /*Offset 0x3A.*/
   u8 linBlueFieldPosition;         /*Offset 0x3B.*/
   u8 linResvMaskSize;              /*Offset 0x3C.*/
   u8 linResvFieldPosition;         /*Offset 0x3D.*/
   u32 maxPixelClock;               /*Offset 0x3E.*/

   u8 reserved4[189 + 1];           /*Offset 0x42.*/
} __attribute__ ((packed)) VBEModeInfo;



#define VBE_INFO_ADDRESS          (0x80000ul + PAGE_OFFSET)
#define VBE_MODE_INFO_ADDRESS     (0x90000ul + PAGE_OFFSET)

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
   u8 *vram = (u8 *)pa2va(currentVBEModeInfo.physBaseAddr);

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
   u8 *vram = (u8 *)(pointer)pa2va(currentVBEModeInfo.physBaseAddr);
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

int writeStringInColor(u8 red,u8 green,u8 blue,const char *string)
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
	    y -= FONT_DISPLAY_HEIGHT;
	 }
      }
   }

   displayPosition = y * xRes + x; /*Update the next display position.*/
   return 0;
}
