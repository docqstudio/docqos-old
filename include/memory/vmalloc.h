#pragma once
#include <core/const.h>

void *vmalloc(u64 size);
int vfree(void *obj); /*See also src/memory/paging.c .*/
