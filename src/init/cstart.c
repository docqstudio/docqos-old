#include <vesa/vesa.h>

int kmain(void)
{
   vesaInit();
   drawAndFillRect(0xFF,0xFF,0xFF,50,300,250,250);
   drawAndFillRect(0x1C,0xED,0x53,70,20,200,100);
   return 0;
}
