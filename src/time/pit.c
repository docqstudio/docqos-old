#include <core/const.h>
#include <time/pit.h>
#include <cpu/io.h>
#include <interrupt/interrupt.h>
#include <video/console.h>

#define PIT_IRQ                0x2
/*Not IRQ0 because we're using APIC.*/
/*It's the same as HPET.*/

#define PIT_FREQ               1193182l

#define PIT_COUNTER0           0x40

#define PIT_CMD                0x43

#define PIT_CMD_BINARY         0x00
#define PIT_CMD_MODE2          0x04
#define PIT_CMD_RW_BOTH        0x30
#define PIT_CMD_COUNTER0       0x00

int initPit(IRQHandler handler,unsigned int hz)
{
   /*PIT of my bochs broken?It can't work successfully on my bochs.*/
   /*But on QEMU or VirtualBox,it works successfully!!!*/

   hz = PIT_FREQ / hz;
   outb(PIT_CMD,PIT_CMD_COUNTER0 | PIT_CMD_BINARY |
                PIT_CMD_MODE2 | PIT_CMD_RW_BOTH);
   outb(PIT_COUNTER0,(u8)hz);
   outb(PIT_COUNTER0,(u8)(hz >> 8));

   if(requestIRQ(PIT_IRQ,handler))
   {
      printkInColor(0xff,0x00,0x00,
         "Failed to initialize PIT!!!\n");
      return -1;
   }
   printk("Initialize PIT successfully!\n");
   return 0;
}
