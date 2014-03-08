#include <core/const.h>
#include <cpu/io.h>
#include <video/console.h>
#include <time/rtc.h>

#define RTC_ADDRESS_REG 0x70
#define RTC_DATA_REG    0x71

#define RTC_REG_SECOND  0x00
#define RTC_REG_MINUTE  0x02
#define RTC_REG_HOUR    0x04
#define RTC_REG_DAY     0x07
#define RTC_REG_MONTH   0x08
#define RTC_REG_YEAR    0x09
#define RTC_REG_STATUSB 0x0b

#define BCD2BIN(bcd) ((bcd & 0x0f) + (bcd >> 4) * 10)

static u8 rtcIn(u8 reg)
{
   outb(RTC_ADDRESS_REG,reg);
   return inb(RTC_DATA_REG);
}

static u64 mktime(u16 year,u8 month,u8 day,u8 hour,u8 minute,u8 second)
{
   if((s8)(month -= 2) <= 0)
   {
      month += 12;
      year -= 1;
   }
   return ((((u64)(year / 4 - year / 100 + year / 400 + 367 * month / 12 + day) +
      year * 365 - 719499) * 24 + hour) * 60 + minute) * 60 + second;
} /*Make the Unix timestamp.*/

int kernelReadRTC(RTCTime *time)
{
   u8 second,minute,hour,day,month,year;
   u8 lsecond,lminute,lhour,lday,lmonth,lyear;

   while(rtcIn(0xa) & (1 << 7))
      asm volatile("pause"); /*Is RTC updating?*/
   second = rtcIn(RTC_REG_SECOND);
   minute = rtcIn(RTC_REG_MINUTE);
   hour = rtcIn(RTC_REG_HOUR);
   day = rtcIn(RTC_REG_DAY);
   month = rtcIn(RTC_REG_MONTH);
   year = rtcIn(RTC_REG_YEAR); /*Read date from RTC.*/
   do{
      lsecond = second;
      lminute = minute;
      lhour = hour;
      lday = day;
      lmonth = month;
      lyear = year;
      while(rtcIn(0xa) & (1 << 7))
         asm volatile("pause"); /*Is RTC updating?*/
      second = rtcIn(RTC_REG_SECOND);
      minute = rtcIn(RTC_REG_MINUTE);
      hour = rtcIn(RTC_REG_HOUR);
      day = rtcIn(RTC_REG_DAY);
      month = rtcIn(RTC_REG_MONTH);
      year = rtcIn(RTC_REG_YEAR); /*Read again.*/
   }while((lsecond != second) || (lminute != minute) || (lhour != hour) ||
      (lday != day) || (lmonth != month) || (lyear != year));
      /*Read until the data is the same.*/

   u8 rb = rtcIn(RTC_REG_STATUSB);

   if(((rb & 2) == 0) && (hour & 0x80))
      hour &= ~0x80;
   else if((rb & 2) == 0)
      rb |= 2;

   if((rb & 4) == 0) /*BCD Code.*/
   {
      year = BCD2BIN(year);
      minute = BCD2BIN(minute);
      second = BCD2BIN(second);
      month = BCD2BIN(month);
      day = BCD2BIN(day);
      hour = BCD2BIN(hour);
   }
   if((rb & 2) == 0)
      hour += 12;
   hour %= 24; 

   time->year = year;
   time->year += 2000; /*Start from 2000.*/
   time->month = month;
   time->day = day;
   time->hour = hour;
   time->minute = minute;
   time->second = second;
           /*Set the time.*/

   return 0;
}

u64 readRTC(void)
{
   RTCTime time;
   kernelReadRTC(&time); /*Read it!*/
   return mktime(time.year,time.month,time.day,
                 time.hour,time.minute,time.second);
             /*Make the UNIX Timestamp.*/
}
