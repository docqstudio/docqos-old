#pragma once
#include <core/const.h>

void *memcpy(void *to,const void *from,int n);
void *memset(void *mem,u8 c,u64 len);
int memcmp(const void *str1,const void *str2,int n);

char *itoa(long long val, char *buf, unsigned int radix,
   char alignType,char alignChar,char isUnsigned);
int strlen(const char *string);
