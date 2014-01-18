#include <core/const.h>
#include <acpi/power.h>
#include <cpu/io.h>
#include <video/console.h>
#include <memory/paging.h>
#include <interrupt/interrupt.h>

/*See also http://www.acpi.info/DOWNLOADS/ACPIspec50.pdf .*/

typedef struct ACPIHeader{
   u32 signature;
   u32 length;
   u8 version;
   u8 checksum;
   u8 oem[6];
   u8 oemTableID[8];
   u32 oemVersion;
   u32 creatorID;
   u32 creatorVersion;
   u32 data[0];
} __attribute__ ((packed)) ACPIHeader;

typedef struct ACPIAddressFormat{
   u8 addressSpaceID;
   u8 registerBitWidth;
   u8 registerBitOffset;
   u8 reserved;
   u64 address;
} __attribute__ ((packed)) ACPIAddressFormat;

/*Section 5.2.9 Fixed ACPI Description Table (FADT).*/
typedef struct ACPIFadt{
   ACPIHeader header;
   u32 firmwareControl;
   u32 dsdt;
   u8 reserved;
   u8 preferredPMProfile;
   u16 sciInterrupt;
   u32 smiCommandPort;
   u8 acpiEnable;
   u8 acpiDisable;
   u8 unused1[56 - 54];
   u32 eventRegister1a;
   u32 eventRegister1b;
   u32 controlRegister1a; /*PM1a_CNT_BLK*/
                         /*System port address of the PM1a Control Register Block.*/
   u32 controlRegister1b; /*PM1b_CNT_BLK*/
                         /*System port address of the PM1b Control Register Block.*/
   u8 unused2[88 - 72];
   u8 eventRegister1Length;
   u8 unused3[116 - 89];
   ACPIAddressFormat resetRegister;
   u8 resetValue;   /*Indicates the value to write to */
                    /*the RESET_REG port to reset the system.*/
   u8 unused4[268 - 129];

} __attribute__ ((packed)) ACPIFadt;

#define SCI_ENABLED    0x1

static const ACPIFadt *acpiFadt = 0;
static const ACPIHeader *acpiSsdt = 0;
static const ACPIHeader *acpiDsdt = 0;

static int acpiPowerIRQ(IRQRegisters *reg,void *data)
{
   u16 status = inw(acpiFadt->eventRegister1a);
   if(status & (1 << 8))
   { /*Section 4.8.3.1.1 Table 4-16*/
     /*1 << 8 :PWRBTN_STS*/
     /*This optional bit is set when the Power Button is pressed.*/
      outw(acpiFadt->eventRegister1a,1 << 8); /*Clear this bit!*/
      doPowerOff();
   }
   if(!acpiFadt->eventRegister1b)
      return 0;
   status = inw(acpiFadt->eventRegister1b);
   if(status & (1 << 8))
   {
      outw(acpiFadt->eventRegister1b,1 << 8); /*Clear this bit.*/
      doPowerOff();
   }
   return 0;
}

static int initAcpiPower(void)
{
   if(!acpiFadt)
      return -1;
   u8 length = acpiFadt->eventRegister1Length;
   length /= 2;
   u32 enableRegister1a = acpiFadt->eventRegister1a + length;
   u32 enableRegister1b = acpiFadt->eventRegister1b + length;
                        /*See Section 4.8.3.1.2 .*/
   if(enableRegister1b == length)
      enableRegister1b = 0;
   
   outw(enableRegister1a,1 << 8);
   if(enableRegister1b)
      outw(enableRegister1b,1 << 8);
      /*Table 4-17*/
      /*1 << 8:PWRBTN_EN*/

   return requestIRQ(acpiFadt->sciInterrupt,&acpiPowerIRQ);
}

int acpiEnable(const void *fadt,const void *ssdt)
{
   if(!acpiSsdt && ssdt)
      acpiSsdt = (const ACPIHeader *)ssdt;
   if(acpiFadt)
      return 0;
   if(!fadt)
      return -1;
   acpiFadt = (const ACPIFadt *)fadt;
   acpiDsdt = (const ACPIHeader *)pa2va(acpiFadt->dsdt);
   if(acpiFadt->smiCommandPort)
      if(acpiFadt->acpiEnable || acpiFadt->acpiDisable)
         outb(acpiFadt->smiCommandPort,acpiFadt->acpiEnable);
         /*Send enable command.*/
   while(!(inw(acpiFadt->controlRegister1a) & SCI_ENABLED))
      asm volatile("pause");  /*Wait for enable.*/
   if(acpiFadt->controlRegister1b)
      while(!(inw(acpiFadt->controlRegister1b) & SCI_ENABLED))
         asm volatile("pause");
   return 0;
}

int doPowerOff(void)
{
   closeInterrupt();
   const ACPIHeader *dt = acpiDsdt;
retry:;
   u8 *data = (u8 *)dt->data;
   u32 length = dt->length - sizeof(*dt);
   for(;length-- > 0;++data)
   {
      if(data[0] != '_' || data[1] != 'S' || data[2] != '5' || data[3] != '_') 
         continue;
      if(data[-1] != 0x8) /*NameOP.*/
         if(data[-1] != '\\' || data[-2] != 0x8)
            continue;
      if(data[4] != 0x12) /*PackageOP.*/
         continue;
      length = 1;
      break;
   }
   if(length != 1)
   {
      if(!acpiSsdt)
         for(;;);
      if(dt == acpiSsdt) 
         for(;;);
      dt = acpiSsdt;
      goto retry;  
         /*First look for \_S5_ in dsdt,then look for \_S5_ in ssdt.*/
   }

   data += 5;
   data += ((data[0] & 0xc0) >> 6) + 2;
   if(data[0] == 0xa) /*Byte Prefix.*/
     ++data;
   u16 s5a = (*data++) << 10;
   if(data[0] == 0xa) /*Byte Prefix.*/
      ++data;
   u16 s5b = (*data++) << 10;

   for(;;)
   {
      outw(acpiFadt->controlRegister1a,s5a | (1 << 13));
      if(acpiFadt->controlRegister1b)
         outw(acpiFadt->controlRegister1b,s5b | (1 << 13));
         /*Send soft off command.*/
   }
   return -1;
}

int doReboot(void)
{
   closeInterrupt();
   if(acpiFadt->header.length < offsetOf(ACPIFadt,unused4[0]))
      goto next;  /*Support reset register?*/
   for(;;)
      outb(acpiFadt->resetRegister.address,acpiFadt->resetValue);
next:
   for(;;)  /*Try to use keyboard controller.*/
   {
      while(inb(0x64) & 0x3)
         inb(0x60);
      outb(0x64,0xfe); /*Send Reset CPU command.*/
   }
   return -1;
}

driverInitcall(initAcpiPower);
