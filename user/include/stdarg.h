#pragma once

typedef __builtin_va_list va_list;

#define va_start(a,b) __builtin_va_start(a,b)

#define va_end(a) __builtin_va_end(a)

#define va_arg(a,b) __builtin_va_arg(a,b)

/*These functions __builtin_* are gived by GCC.*/
/*Although we used -fno-builtin,we are also able to use them.*/
