#include <core/const.h>
#include <memory/paging.h>
#include <cpu/io.h>
#include <cpu/spinlock.h>
#include <lib/string.h>
#include <driver/serialport.h>
#include <time/rtc.h>

#define MAX_SERIAL_PORTS 4

u16 serialPorts[MAX_SERIAL_PORTS + 1];
SpinLock serialPortLock;

static inline int outSerialPort(u16 port,u8 data)
{
   while((inb(port + 5) & 0x20) == 0)
      asm volatile("pause"); 
      /*Wait until we can send the data to the serial port.*/
   outb(port + 0,data);
   return 0;
}

static inline int initSerialPort(u16 port)
{
   outb(port + 1, 0x00); /*Disable all interrupts.*/
   outb(port + 3, 0x80); /*Enable DLAB (set baud rate divisor).*/
   outb(port + 0, 0x03); /*Set divisor to 3 (lo byte) 38400 baud. */
   outb(port + 1, 0x00); /*                 (hi byte) */
   outb(port + 3, 0x03); /*8 bits, no parity, one stop bit. */
   outb(port + 2, 0xC7); /*Enable FIFO, clear them, with 14-byte threshold. */
#if 0
   outb(PORT + 4, 0x0B); /*IRQs enabled, RTS/DSR set. */
#endif /*We don't enable the IRQS!*/
   return 0;
}

int initSerialPorts(void)
{
   u16 *ports = pa2va(0x400);
   memset(serialPorts,0,sizeof(serialPorts));
   for(int i = 0,j = 0;i < MAX_SERIAL_PORTS;++i)
      if(ports[i])
         serialPorts[j++] = ports[i];

   ports = serialPorts;
   while(*ports)
      initSerialPort(*ports++);
   initSpinLock(&serialPortLock);

   return 0;
}

int writeSerialPort(const char *data,u8 nr)
{
   static char last = '\n';
   static const char *const months[] = 
      {
         "Jan","Feb","Mar",
         "Apr","May","Jun",
         "Jul","Aug","Sep",
         "Oct","Nov","Dec"
      };
   char p[] = "Jan  1 00:00:00 localhost kernel:";
   const char *month;
   char c;
   u16 port;
   RTCTime time;
   if(nr >= 4)
      return -EINVAL;
   port = serialPorts[nr];
   if(!port) /*No such serial port.*/
      return -ENOENT;

   kernelReadRTC(&time);
   month = months[time.month - 1];
   for(int i = 0;i < 3;++i)
      p[i] = month[i];
   p[5] = (time.day % 10) + '0';
   if((time.day /= 10))
      p[4] = time.day + '0';
   p[8] = (time.hour % 10) + '0';
   if((time.hour /= 10))
      p[7] = time.hour + '0';
   p[11] = (time.minute % 10) + '0';
   if((time.minute /= 10))
      p[10] = time.minute + '0';
   p[14] = (time.second % 10) + '0';
   if((time.second /= 10))
      p[13] = time.second + '0';

   lockSpinLock(&serialPortLock);
   while((c = *data++))
   {
      if(last == '\n')
      {
         for(int i = 0;p[i];++i)
            outSerialPort(port,p[i]);
             /*Write p[i] to the serial port.*/
      }
      outSerialPort(port,c);
      last = c;
   }
   unlockSpinLock(&serialPortLock);
   return 0;
}

