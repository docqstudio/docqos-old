#pragma once
#include <core/const.h>

typedef __builtin_va_list VarArgsList;

#define varArgsStart(a,b) __builtin_va_start(a,b)

#define varArgsEnd(a) __builtin_va_end(a)

#define varArgsNext(a,b) __builtin_va_arg(a,b)

/*These functions __builtin_* are gived by GCC.*/
/*Although we used -fno-builtin,we are also able to use them.*/
