#pragma once
#include <core/const.h>

typedef struct BlockDevicePart BlockDevicePart;

int devfsRegisterBlockDevice(BlockDevicePart *part,const char *name);
