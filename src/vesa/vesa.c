#include <vesa/vesa.h>
#include <core/const.h>
#include <lib/string.h>

static VBEInfo currentVBEInfo = {};
static VBEModeInfo currentVBEModeInfo = {};

int null(long x){return (int)x;}

int vesaInit(void)
{
   memcpy((void *) &currentVBEInfo,
      (const void *) (VBE_INFO_ADDRESS),
      sizeof(VBEInfo));
   memcpy((void *) &currentVBEModeInfo,
      (const void *) (VBE_MODE_INFO_ADDRESS),
      sizeof(VBEModeInfo));
   return 0; /*Successful.*/
}

int drawAndFillRect(
   u8 red,u8 green,u8 blue,int x,int y,int width,int height)
{
   u32 color;
   u8 fill1,fill2,fill3;
   const u32 xRes = currentVBEModeInfo.xResolution;
   const u32 yRes = currentVBEModeInfo.yResolution;
   u8 *vram = (u8 *)currentVBEModeInfo.physBaseAddr;

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
