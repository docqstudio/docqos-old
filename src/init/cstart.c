#include <vesa/vesa.h>

int kmain(void)
{
   initVESA(); /*Init vesa.*/
   
   writeColorString(0x00,0xFF,0x00, /*Green.*/
      "------------------kmain started------------------\n");
   writeString("Initialize VESA sucessfully.\n");

   return 0;
}
