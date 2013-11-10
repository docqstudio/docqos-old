#include <core/const.h>
#include <cpu/io.h>
#include <video/console.h>
#include <interrupt/pic.h>
#include <interrupt/idt.h>
#include <interrupt/localapic.h>
#include <interrupt/ioapic.h>

int initInterrupt(void)
{
   initIDT();
   initPIC(); /*In fact,it will disable PIC.*/

   if(initLocalApic())
      return -1;

   if(initIOApic())
      return -1;

   startInterrupt();

   printkInColor(0x00,0xff,0x00,"Initialize interrupt successfully!\n");
   return 0;
}
