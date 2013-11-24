#pragma once
#include <core/const.h>
#include <interrupt/interrupt.h>

int initHpet(IRQHandler handler,unsigned int hz);
