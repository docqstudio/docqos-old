#pragma once

#include <core/const.h>

typedef struct MultibootTagFrameBuffer MultibootTagFrameBuffer;

int initFrameBuffer(MultibootTagFrameBuffer *fb);

inline int frameBufferDrawPoint(u8 red,u8 green,u8 blue,int x,int y) __attribute__((always_inline));
inline int frameBufferWriteString(const char *string) __attribute__ ((always_inline));

int frameBufferFillRect(
   u8 red,u8 green,u8 blue,int x,int y,int width,int height);
int frameBufferDrawChar(u8 red,u8 green,u8 blue,int x,int y,unsigned char charDrawing);
int frameBufferDrawString(u8 red,u8 green,u8 blue,int x,int y,const char *string);
int frameBufferWriteStringInColor(u8 red,u8 green,u8 blue,const char *string);

inline int frameBufferDrawPoint(u8 red,u8 green,u8 blue,int x,int y){
   return frameBufferFillRect(red,green,blue,x,y,0x1,0x1);
}

inline int frameBufferWriteString(const char *string){
   return frameBufferWriteStringInColor(0xff,0xff,0xff,string);
}
