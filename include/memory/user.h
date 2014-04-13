#pragma once
#include <core/const.h>

/* #define UserSpace(type) type __attribute__((noderef,address_space(1))) */
          /*Need sparse?*/
#define UserSpace(type) type

#include <task/task.h>

inline int verifyUserAddress(const void *__address,unsigned long size)
    __attribute__ ((always_inline));
inline unsigned long getAddressLimit(void) __attribute__ ((always_inline));
inline int setKernelAddressLimit(void) __attribute__ ((always_inline));
inline int setUserAddressLimit(void) __attribute__ ((always_inline));
inline int setAddressLimit(unsigned long limit) __attribute__ ((always_inline));

inline unsigned long getAddressLimit(void)
{
   return getCurrentTask()->addressLimit;
}

inline int setAddressLimit(unsigned long limit)
{
   getCurrentTask()->addressLimit = limit;
   return 0;
}

inline int setKernelAddressLimit(void)
{
   return setAddressLimit(~0ul);
}

inline int setUserAddressLimit(void)
{
   return setAddressLimit(0x8000000000ul);
}

inline int verifyUserAddress(const void *__address,unsigned long size)
{
   unsigned long address = (unsigned long)__address;
   if(address + size < address)
      return -EFAULT;
   if(address + size >= getCurrentTask()->addressLimit)
      return -EFAULT;
   return 0;
}

int getUser8(UserSpace(const void) *address,unsigned char *retval)
          __attribute__ ((warn_unused_result)); /*We must check the results of these function!!!*/
int getUser32(UserSpace(const void) *address,unsigned int *retval)
          __attribute__ ((warn_unused_result));
int getUser64(UserSpace(const void) *address,unsigned long *retval)
          __attribute__ ((warn_unused_result));
int putUser8(UserSpace(void) *address,unsigned char content)
          __attribute__ ((warn_unused_result));
int putUser32(UserSpace(void) *address,unsigned int content)
          __attribute__ ((warn_unused_result));
int putUser64(UserSpace(void) *address,unsigned long content)
          __attribute__ ((warn_unused_result));

#define makeSafeUserFunction(name,l0,l1,type) \
   inline int name##Safe (l0 a,l1 b) \
      __attribute__ ((always_inline,warn_unused_result)); \
   inline int name##Safe (l0 a,l1 b) \
   { \
      if(verifyUserAddress(a,sizeof(type))) \
         return -EFAULT; \
      return name(a,b); \
   }

makeSafeUserFunction(getUser8,UserSpace(const void) *,unsigned char *,unsigned char);
makeSafeUserFunction(getUser64,UserSpace(const void) *,unsigned long *,unsigned long);
makeSafeUserFunction(getUser32,UserSpace(const void) *,unsigned int *,unsigned int);
makeSafeUserFunction(putUser8,UserSpace(void) *,unsigned char,unsigned char);
makeSafeUserFunction(putUser64,UserSpace(void) *,unsigned long,unsigned long);
makeSafeUserFunction(putUser32,UserSpace(void) *,unsigned int,unsigned int);

#undef makeSafeUserFunction

#define getUserPointerSafe(address,retval) getUser64Safe(address,(unsigned long *)retval)
#define getUserPointer(address,retval) getUser64(address,(unsigned long *)retval)

#define putUserPointerSafe(address,content) putUser64Safe(address,(unsigned long)content)
#define putUserPointer(address,content) putUser64(address,(unsigned long)content)

int memcpyUser1(void *dest,UserSpace(const void) *src,unsigned long n)
          __attribute__ ((warn_unused_result));
int memcpyUser0(UserSpace(void) *dest,const void * src,unsigned long n)
          __attribute__ ((warn_unused_result));

long strncpyUser1(char *dest,UserSpace(const char) *src,unsigned long n)
          __attribute__ ((warn_unused_result));
long strncpyUser0(UserSpace(char) *dest,const char *src,unsigned long n)
          __attribute__ ((warn_unused_result));
