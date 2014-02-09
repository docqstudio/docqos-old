#pragma once

typedef struct VFSFile VFSFile;
typedef u8 (KeyboardCallback)(u8 data);

int ttyWrite(VFSFile *file,const void *string,u64 data);
int ttyRead(VFSFile *file,void *string,u64 data);
int ttyKeyboardPress(KeyboardCallback *callback,u8 data);
