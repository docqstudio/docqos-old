#pragma once

void *memcpy(void *to,const void *from,int n);

char *itoa(long long val, char *buf, unsigned int radix,
   char alignType,char alignChar,char isUnsigned);
int strlen(const char *string);
