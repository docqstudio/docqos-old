#pragma once

#include <core/const.h>

typedef struct MultibootTagFrameBuffer MultibootTagFrameBuffer;

int initFrameBuffer(MultibootTagFrameBuffer *fb);

inline int frameBufferWriteString(const char *string) __attribute__ ((always_inline));

int frameBufferFillRect(
   u8 red,u8 green,u8 blue,int x,int y,int width,int height);
int frameBufferDrawChar(u8 bred,u8 bgreen,u8 bblue,
     u8 red,u8 green,u8 blue,int x,int y,unsigned char charDrawing);
int frameBufferDrawString(u8 red,u8 green,u8 blue,int x,int y,const char *string);
int frameBufferWriteStringInColor(u8 red,u8 green,u8 blue,const char *string,
                     u64 size,u8 refresh);
int frameBufferScroll(int numberOfLine);
int frameBufferRefreshLine(unsigned int line,unsigned int vaild);
void *createFrameBufferLayer(unsigned int x,unsigned int y,
                   unsigned int width,unsigned int height,unsigned char refresh);
int moveFrameBufferLayer(void *__layer,unsigned int x,
         unsigned int y,unsigned int fromNow,unsigned char refresh);

inline int frameBufferWriteString(const char *string)
{
   return frameBufferWriteStringInColor(0xff,0xff,0xff,string,0,1);
}
