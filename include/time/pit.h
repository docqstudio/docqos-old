#pragma once
#include <core/const.h>
#include <interrupt/interrupt.h>

int initPit(IRQHandler handler,unsigned int hz);
