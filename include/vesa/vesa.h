#pragma once

#include <core/const.h>

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

inline int drawPoint(u8 red,u8 green,u8 blue,int x,int y) __attribute__((always_inline));
inline int writeString(const char *string) __attribute__ ((always_inline));

int fillRect(
   u8 red,u8 green,u8 blue,int x,int y,int width,int height);
int initVESA(void);
int drawChar(u8 red,u8 green,u8 blue,int x,int y,unsigned char charDrawing);
int drawString(u8 red,u8 green,u8 blue,int x,int y,const char *string);
int writeColorString(u8 red,u8 green,u8 blue,const char *string);

inline int drawPoint(u8 red,u8 green,u8 blue,int x,int y){
   return fillRect(red,green,blue,x,y,0x1,0x1);
}

inline int writeString(const char *string){
   return writeColorString(0xFF,0xFF,0xFF,string);
}
