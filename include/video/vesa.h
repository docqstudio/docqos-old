#pragma once

#include <core/const.h>

inline int drawPoint(u8 red,u8 green,u8 blue,int x,int y) __attribute__((always_inline));
inline int writeString(const char *string) __attribute__ ((always_inline));

int fillRect(
   u8 red,u8 green,u8 blue,int x,int y,int width,int height);
int initVESA(void);
int drawChar(u8 red,u8 green,u8 blue,int x,int y,unsigned char charDrawing);
int drawString(u8 red,u8 green,u8 blue,int x,int y,const char *string);
int writeStringInColor(u8 red,u8 green,u8 blue,const char *string);

inline int drawPoint(u8 red,u8 green,u8 blue,int x,int y){
   return fillRect(red,green,blue,x,y,0x1,0x1);
}

inline int writeString(const char *string){
   return writeStringInColor(0xFF,0xFF,0xFF,string);
}
