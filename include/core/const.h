#pragma once

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;
typedef u64 pointer;
/*NOTE: sizeof(pointer) == sizeof(void *).*/

#define containerOf(ptr,type,member) ({ \
   const typeof(((type *)0)->member) *___ = (ptr);\
   (type *)((char *)___ - offsetOf(type,member));\
})

#define offsetOf(type,member) \
   ((pointer)&((type *)0)->member)

#define CONFIG_DEBUG
