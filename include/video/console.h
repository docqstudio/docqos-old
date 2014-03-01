#pragma once
#include <core/const.h>

int printk(const char *string, ...) __attribute__ ((format(printf,1,2)));
int printkInColor(u8 red,u8 green,u8 blue,const char *string, ...)
                 __attribute__ ((format(printf,4,5)));
