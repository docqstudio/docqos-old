#pragma once
#include <core/const.h>

typedef struct KeyboardState
{
   u8 caps:1;
   u8 num:1;
   u8 scroll:1;
   u8 shift:1;
   u8 ctrl:1;
   u8 data:1;
} KeyboardState;

typedef struct VFSFile VFSFile;
typedef u8 (KeyboardCallback)(u8 data,KeyboardState *state);

#define KEY_UP    0x81
#define KEY_DOWN  0x82
#define KEY_LEFT  0x83
#define KEY_RIGHT 0x84

int ttyWrite(VFSFile *file,const void *string,u64 data);
int ttyRead(VFSFile *file,void *string,u64 data);
int ttyKeyboardPress(KeyboardCallback *callback,u8 data);
