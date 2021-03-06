#pragma once
#include <core/const.h>
#include <driver/driver.h>

typedef struct PCIDevice
{
   u32 class;
   u16 vendor,device;
   u16 interrupt;
   u32 bar[6];

   struct{
      u8 function,device,bus;
   } position;

   Device globalDevice;
} PCIDevice;

