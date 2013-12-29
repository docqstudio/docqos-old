#pragma once
#include <core/const.h>
#include <core/list.h>

typedef struct BlockDevicePart BlockDevicePart;
typedef struct FileSystem FileSystem;

typedef enum BlockDeviceType{
   InvaildBlockDevice,
   BlockDeviceDisk,
   BlockDeviceCDROM
} BlockDeviceType;

typedef struct BlockDevice{
   void *data;  
   int (*read)(void *data,u64 start,u64 size,void *buf);
   int (*write)(void *data,u64 start,u64 size,void *buf);
   u64 end;

   BlockDeviceType type;
   ListHead list;

   BlockDevicePart *parts;
   int partCount;
} BlockDevice;

typedef struct BlockDevicePart{
   BlockDevice *device;
   u64 start;
   u64 end;

   FileSystem *fileSystem;
   BlockDevicePart *next; 
      /*BlockDevice.*/
   ListHead list;
} BlockDevicePart;

typedef struct BlockIO{
   BlockDevicePart *part;
   u64 start;
   u64 size;
   void *buffer;
   int read; /*0:write,1:read.*/
} BlockIO;

int submitBlockIO(BlockIO *io);
int registerBlockDevice(BlockDevice *device);
