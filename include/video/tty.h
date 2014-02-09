#pragma once

typedef struct VFSFile VFSFile;

int ttyWrite(VFSFile *file,const void *string,u64 data);
int ttyRead(VFSFile *file,void *string,u64 data);
