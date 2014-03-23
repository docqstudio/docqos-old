#pragma once

typedef struct VFSFile VFSFile;
typedef u8 (KeyboardCallback)(u8 data,u8 *shift,u8 *ctrl);

#define KEY_UP    0x81
#define KEY_DOWN  0x82
#define KEY_LEFT  0x83
#define KEY_RIGHT 0x84

int ttyWrite(VFSFile *file,const void *string,u64 data);
int ttyRead(VFSFile *file,void *string,u64 data);
int ttyKeyboardPress(KeyboardCallback *callback,u8 data);
