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

typedef int (*InitcallFunction)(void);

#define DEFINE_INITCALL(id,fn) \
   static InitcallFunction __##fn##Initcall##id \
   __attribute__ ((section(".init.initcall" #id),used)) \
   = &fn

#define subsysInitcall(fn) DEFINE_INITCALL(0,fn)
#define driverInitcall(fn) DEFINE_INITCALL(1,fn)

#define CONFIG_DEBUG
