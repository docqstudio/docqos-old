#include <core/const.h>
#include <driver/pci.h>
#include <driver/driver.h>
#include <cpu/io.h>
#include <video/console.h>
#include <memory/kmalloc.h>

#define PCI_DATA_REG             0x0cfc
#define PCI_CMD_REG              0x0cf8

#define PCI_CMD_VENDOR_WORD      0x0000
#define PCI_CMD_DEVICE_WORD      0x0002

#define PCI_CMD_REVISION_BYTE    0x0008
#define PCI_CMD_PROGIF_BYTE      0x0009
#define PCI_CMD_SUBCLASS_BYTE    0x000a
#define PCI_CMD_CLASS_BYTE       0x000b
#define PCI_CMD_HEADER_BYTE      0x000e

#define PCI_CMD_BAR_LONG(bar)    (0x0010 + (bar)*0x4)

static inline u32 pciInl(u16 bus,u16 device,u16 function,u16 offset)
   __attribute__ ((always_inline));
static inline u16 pciInw(u16 bus,u16 device,u16 function,u16 offset)
    __attribute__ ((always_inline));
static inline u8 pciInb(u16 bus,u16 device,u16 function,u16 offset)
    __attribute__ ((always_inline));

static inline u32 pciInl(u16 bus,u16 device,u16 function,u16 offset)
{
   u32 address = 0x80000000; /*Enable bit.*/
   address |= (offset & 0xfc);
   address |= (function << 8);
   address |= (device << 11);
   address |= (bus << 16);
   outl(PCI_CMD_REG,address);
   return inl(PCI_DATA_REG);
}
static inline u16 pciInw(u16 bus,u16 device,u16 function,u16 offset)
{
   return (pciInl(bus,device,function,offset) >> ((offset & 2) * 8)) & 0xffff;
}

static inline u8 pciInb(u16 bus,u16 device,u16 function,u16 offset)
{
   return (pciInw(bus,device,function,offset) >> ((offset & 1) *8)) & 0x00ff;
}

static int parseFunction(u16 bus,u16 device,u16 function)
{
   static const char * classes[] = {
       [0x00] = "Unspecified Type",
       [0x01] = "Mass Storage Controller",
       [0x02] = "Network Controller",
       [0x03] = "Display Controller",
       [0x04] = "Multimedia Controller",
       [0x05] = "Memory Controller",
       [0x06] = "Bridge Device",
       [0x07] = "Simple Communication Controller",
       [0x08] = "Base System Peripheral",
       [0x09] = "Input Device",
       [0x0A] = "Docking Station",
       [0x0B] = "Processor",
       [0x0C] = "Serial Bus Controller",
       [0x0D] = "Wireless Controller",
       [0x0E] = "Intelligent I/O Controller",
       [0x0F] = "Satellite Communication Controller",
       [0x10] = "Encryption/Decryption Controller",
       [0x11] = "Data Acquisition and Signal Processing Controller",

       [0x12 ... 0xfe] = "Unknow Type",

       [0xff] = "Miscellaneous Device"
   };

   PCIDevice *pci = kmalloc(sizeof(PCIDevice));
   if(!pci)
   {
      printkInColor(0xff,0x00,0x00,"(%s)Function kmalloc failed,no memory?\n",__func__);
      return -1;
   }
   pci->position.bus = bus;
   pci->position.function = function;
   pci->position.device = device; /*Tag it.*/

   u8 class = pciInb(bus,device,function,PCI_CMD_CLASS_BYTE);
   u8 subClass = pciInb(bus,device,function,PCI_CMD_SUBCLASS_BYTE);
   u8 progif = pciInb(bus,device,function,PCI_CMD_PROGIF_BYTE);
   u8 revision = pciInb(bus,device,function,PCI_CMD_REVISION_BYTE);

   pci->class = (class << 24) | (subClass << 16) | (progif << 8) | (revision << 0);

   for(int bar = 0;bar < 6;++bar)
      pci->bar[bar] = pciInl(bus,device,function,PCI_CMD_BAR_LONG(bar));

   pci->globalDevice.type = DeviceTypePCI;

   pci->vendor = pciInw(bus,device,function,PCI_CMD_VENDOR_WORD);
   pci->device = pciInw(bus,device,function,PCI_CMD_DEVICE_WORD);
   /*Get some information.*/

   registerDevice(&pci->globalDevice); /*And register this device.*/

   printk("Found device from PCI:");
   printk("Class Type:%s,",classes[class]);
   printk("Sub Class:%d\n",subClass);
   /*Print some information.*/
   return 0;
}

static int parseDevice(u16 bus,u16 device)
{
   u16 vendor = pciInw(bus,device,0,PCI_CMD_VENDOR_WORD);
   if(vendor == 0xffff)
      return -1; /*It doesn't exist!*/
   
   parseFunction(bus,device,0);

   u8 header = pciInb(bus,device,0,PCI_CMD_HEADER_BYTE);
   if(header & 0x80) /*This device has more functions?*/
      for(int function = 1;function < 8;++function)
         if(pciInw(bus,device,function,PCI_CMD_VENDOR_WORD) != 0xffff)
            parseFunction(bus,device,function); /*Parse it!*/
   return 0;
}

static int parseBus(u16 bus)
{
   for(int device = 0;device < 32;++device)
      parseDevice(bus,device); /*Foreach 32 devices.*/
   return 0;
}

static int initPCI(void)
{
   outl(PCI_CMD_REG,0x80000000);
   if(inl(PCI_CMD_REG) != 0x80000000)
   { /*PCI doesn't exist.*/
      printkInColor(0xff,0x00,0x00,"PCI doesn't exist!\n");
      return -1; /*Error.*/
   }

   u8 header = pciInb(0x0,0x0,0x0,PCI_CMD_HEADER_BYTE);

   if(header & 0x80) /*Mulitiple PCI host controllers?*/
      for(int bus = 0;bus < 8;++bus)
      {
         if(pciInw(bus,0x0,0x0,PCI_CMD_VENDOR_WORD) != 0xffff)
            break; /*No more buses.*/
         parseBus(bus);
      }
   else
      parseBus(0); /*Only one bus.*/
   return 0;
}

subsysInitcall(initPCI);
