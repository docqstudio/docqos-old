#pragma once
#include <core/const.h>

void *memcpy(void *to,const void *from,int n);
void *memset(void *mem,u8 c,u64 len);

char *itoa(long long val, char *buf, unsigned int radix,
   char alignType,char alignChar,char isUnsigned);
int strlen(const char *string);
