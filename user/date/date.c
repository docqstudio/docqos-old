#include <unistd.h>

static inline signed long divide(signed long d1,signed long d2)
   __attribute__ ((always_inline));
static inline int isLeapYear(signed long year)
   __attribute__ ((always_inline));

static inline int isLeapYear(signed long year)
{
   return (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
}


static inline signed long divide(signed long d1,signed long d2)
{
   return (d1 / d2 - (d1 % d2 < 0));
}

/*This function return how many leap years there are betwen 'y1' and 'y2'.*/
static inline signed long getLeapYears(signed long y1,signed long y2)
{
   signed long d1 = divide(y1 - 1,4) - divide(y1 - 1,100) + divide(y1 - 1,400);
   signed long d2 = divide(y2 - 1,4) - divide(y2 - 1,100) + divide(y2 - 1,400);
   signed long d = d1 - d2;
   return (d > 0) ? d : -d;
}

static int gmtime(unsigned long time,unsigned long result[])
{
   static const unsigned short monthDays[2][13] = {
      {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365},
      {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366}
   };
   const unsigned short *id;
   signed long year = 1970; /*Start from 1970-1-1 00:00:00 .*/
   signed long days = time / (60 * 60 * 24); /*SECONDS_PER_DAY .*/
   signed long rem = time % (60 * 60 * 24);
   result[4] = rem / (60 * 60); /*SECONDS_PER_HOUR .*/
                          /*result[4] : hour .*/
   rem %= 60 * 60;
   result[5] = rem / 60;  /*reuslt[5] : minute .*/
   result[6] = rem % 60;  /*result[6] : second .*/

   result[3] = (4 + days) % 7; /*1970-1-1 is Thursday.*/
   
   while(days < 0 || 
      (days >= (isLeapYear(year) ? 366 : 365)))
   {
      signed long y = year + divide(days,365);
      days -= (y - year) * 365;
      days -= getLeapYears(year,y);
      year = y;
   }
   result[0] = year; /*result[0] : year*/

   id = monthDays[isLeapYear(year)];
   /*Now the 'year' is the 'month'.*/
   for(year = 11;days < id[year];--year)
      ;
   days -= id[year];
   result[1] = year + 1; /*reuslt[1] : month .*/
   result[2] = days + 1; /*result[2] : day .*/
   return 0;
}

int main(void)
{
   static const char *const wdays[] =
      {"Sunday   ","Monday   ","Tuesday  ","Wednesday","Thursday ","Friday   ","Saturday "};
   char string[40];
   unsigned long result[7];
   unsigned long time;
   gettimeofday(&time,0);
   gmtime(time,&result[0]);
   
   for(int i = 0,j = 1000;i < 4;++i,j /= 10)
      string[i + 0] = result[0] / j % 10 + '0'; /*Year.*/
   string[4] = '-';
   for(int i = 0,j = 10;i < 2;++i,j /= 10)
      string[i + 5] = result[1] / j % 10 + '0'; /*Month.*/
   string[7] = '-';
   for(int i = 0,j = 10;i < 2;++i,j /= 10)
      string[i + 8] = result[2] / j % 10 + '0'; /*Day.*/
   string[10] = ' ';

   for(int i = 0,j = 10;i < 2;++i,j /= 10)
      string[i + 11] = result[4] / j % 10 + '0'; /*Hour.*/
   string[13] = ':';
   for(int i = 0,j = 10;i < 2;++i,j /= 10)
      string[i + 14] = result[5] / j % 10 + '0'; /*Minute.*/
   string[16] = ':';
   for(int i = 0,j = 10;i < 2;++i,j /= 10)
      string[i + 17] = result[6] / j % 10 + '0'; /*Second.*/
   string[19] = ' ';

   const char *wday = wdays[result[3]];
   for(int i = 0;i < 9;++i)
      string[i + 20] = wday[i];
   string[29] = '\n';
   string[30] = '\0'; /*Set end.*/

   write(stdout,string,0);

   return 0;
}
