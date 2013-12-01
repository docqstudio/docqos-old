#pragma once
#include <core/const.h>

typedef struct SpinLock{
   volatile u8 lock;
} SpinLock;
