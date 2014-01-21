#pragma once
#include <core/const.h>
#include <core/list.h>

typedef int (*TimerCallBackFunction)(void *arg);

typedef struct Timer{
   TimerCallBackFunction callback;
   void *data;
   int onStack;
   unsigned long long ticks;
   ListHead list;
} Timer;

#define TIMER_HZ     100
#define MSEC_PER_SEC 1000

int initTimer(Timer *timer,
   TimerCallBackFunction callback,int timeout,void *data);
Timer *createTimer(TimerCallBackFunction callback,
   int timeout,void *data);
int addTimer(Timer *timer);
int removeTimer(Timer *timer);

int doGetTimeOfDay(u64 *time,void *unused);
unsigned long long getTicks(void);

int initTime(void);

