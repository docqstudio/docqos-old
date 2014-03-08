#pragma once

typedef struct RTCTime
{
   u16 year;
   u8 month;
   u8 day;
   u8 hour;
   u8 minute;
   u8 second;
} RTCTime;

int kernelReadRTC(RTCTime *time);
u64 readRTC(void);
