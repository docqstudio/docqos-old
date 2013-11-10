#include <core/const.h>
#include <interrupt/pic.h>
#include <cpu/io.h>

#define PIC1_CMD  0x0020
#define PIC2_CMD  0x00a0
#define PIC1_DATA 0x0021
#define PIC2_DATA 0x00a1


int initPIC(void)
{
   outb(PIC1_CMD,0x11);
   outb(PIC2_CMD,0x11);

   outb(PIC1_DATA,0x20);
   outb(PIC2_DATA,0x28);

   outb(PIC1_DATA, 1 << 2);
   outb(PIC2_DATA, 2);

   outb(PIC1_DATA, 0x01);
   outb(PIC2_DATA, 0x01);

   outb(PIC1_DATA, 0xff);
   outb(PIC2_DATA, 0xff);
   return 0;
}
