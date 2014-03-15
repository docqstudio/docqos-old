#pragma once
#include <core/const.h>

inline u64 max(u64 a,u64 b) __attribute__ ((always_inline));
inline u64 min(u64 a,u64 b) __attribute__ ((always_inline));

inline u64 max(u64 a,u64 b)
{
   return (a > b) ? a : b;
}

inline u64 min(u64 a,u64 b)
{
   return (a > b) ? b : a;
}
