#include <core/const.h>
#include <lib/string.h>
#include <acpi/acpi.h>
#include <video/console.h>

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

typedef struct ACPIHeaderApic{
   ACPIHeader header;
   u32 localApicAddress;
   u32 flags;
   u8 data[0];
} __attribute__ ((packed)) ACPIHeaderApic;

typedef struct ApicHeader{
   u8 type;
   u8 length;
} __attribute__ ((packed)) ApicHeader;

typedef struct LocalApic{
   ApicHeader header;
   u8 apicProcessorID;
   u8 apicID;
   u32 flags;
} __attribute__ ((packed)) LocalApic;

typedef struct IOApic{
   ApicHeader header;
   u8 ioApicID;
   u8 reserved;
   u32 ioApicAddress;
   u32 globalSystemInterruptBase;
} __attribute__ ((packed)) IOApic;

#define APIC_TYPE_LOCAL_APIC         0x0
#define APIC_TYPE_IO_APIC            0x1
#define APIC_TYPE_INTERRUPT_OVERRIDE 0x2

static u8 *localApicAddress = 0;
static u8 *ioApicAddress = 0;

static int parseApic(ACPIHeaderApic *apic)
{
   char temp[20];
   temp[0] = '0';temp[1] = 'x';
   itoa(apic->localApicAddress,temp + 2,0x10,8,'0',1);
   printk("\nLocal Apic Address: %s\n",temp);
   localApicAddress = (u8 *)(pointer)apic->localApicAddress;

   u8 *start = apic->data;
   u8 *end = ((u8 *)apic) + apic->header.length;
   while(start < end)
   {
      ApicHeader *apicHeader = (ApicHeader *)start;
      u8 type = apicHeader->type;
      u8 length = apicHeader->length;
      switch(type)
      {
      case APIC_TYPE_LOCAL_APIC:
         {
	    LocalApic *localApic = (LocalApic *)apicHeader;
	    printk("Found CPU: Processor ID => %d, Apic ID => %d \n",
	       (int)localApic->apicProcessorID,(int)localApic->apicID);
            break;
	 }
      case APIC_TYPE_IO_APIC:
         {
	    IOApic *ioApic = (IOApic *)apicHeader;
	    itoa(ioApic->ioApicAddress,temp + 2,0x10,8,'0',1);
            printk("Found I/O Apic : I/O Apic ID => %d, I/O Apic Address => %s\n",
	       (int)ioApic->ioApicID,temp);
	    ioApicAddress = (u8 *)(pointer)ioApic->ioApicAddress;
            break;
	 }
      case APIC_TYPE_INTERRUPT_OVERRIDE:
         break;
      default:
         printk("Unknow Apic information type:%d,length:%d.\n",(int)type,(int)length);
	 break;
      }
      start += length;
   }
   printk("\n");
   return 0;
}

static int parseDT(ACPIHeader *dt)
{
   u32 signature = dt->signature;
   char signatureString[5];
   memcpy((void *)signatureString,(const void *)&signature,4);
   signatureString[4] = '\0';
   printk("Found device %s from ACPI.\n",signatureString);

   if(signature == *(u32 *)"APIC")
      parseApic((ACPIHeaderApic *)dt);
   return 0;
}

static int parseXSDT(ACPIHeader *xsdt)
{
   u32 *start = xsdt->data;
   u32 *end = (u32 *)((u8 *)xsdt + xsdt->length);
   while(start < end)
   {
      u32 dt = *(start++);
      parseDT((ACPIHeader *)(pointer)dt);
   }
   return 0;
}

static int parseRSDT(ACPIHeader *rsdt)
{
   u32 *start = rsdt->data;
   u32 *end = (u32 *)((u8 *)rsdt + rsdt->length);
   while(start < end)
   {
      u32 dt = *(start++);
      parseDT((ACPIHeader *)(pointer)dt);
   }
   return 0;
}

static int parseRSDP(u8 *rsdp)
{
   u8 sum = 0;
   for(int i = 0;i < 20;++i)
   {
      sum += rsdp[i];
   }
   if(sum)
   {
      return -1;
   }
   printkInColor(0x00,0xff,0x00,"ACPI is found!!\n");

   printk("ACPI Information:");
   {
      char oem[7];
      memcpy((void *)oem,(const void *)rsdp + 9,sizeof(oem)/sizeof(char) - 1);
      oem[6] = '\0';
      printk("OEM = %s,",oem);
   }
   u8 version = rsdp[15];
   printk("Version = %d\n",(int)version);
   if(version == 0)
   {
      u32 rsdt = *(u32 *)(rsdp + 16);
      parseRSDT((ACPIHeader *)(pointer)rsdt);
   }else if(version == 2)
   {
      u64 xsdt = *(u64 *)(rsdp + 24);
      u32 rsdt = *(u32 *)(rsdp + 16);
      if(xsdt)
         parseXSDT((ACPIHeader *)(pointer)xsdt);
      else
         parseRSDT((ACPIHeader *)(pointer)rsdt);
   }else{
      printkInColor(0xff,0x00,0x00,"\nUnknow ACPI's version!!!\n");
      return -1;
   }

   printk("\n");

   return 0;
}

u8 *getLocalApicAddress(void)
{
   return localApicAddress;
}

u8 *getIOApicAddress(void)
{
   return ioApicAddress;
}

int initACPI(void)
{
   u8 *start = (u8 *)0xe0000;
   u8 * const end   = (u8 *)0xfffff; /*BIOS read-only area.*/
   while(start < end){
      u64 signature = *(u64 *)start;
      if(signature == *(const u64 *)"RSD PTR ")
      {
         if(parseRSDP(start) == 0)
	 {
	    return 0;
	 }
      }
      start += 0x10;
   }
   u8 *ebda = (u8 *)(pointer)((*((u16 *)0x40E))*0x10);
   if(ebda != (u8 *)0x9FC00)
      return -1;
   u8 * const ebdaEnd = ebda + 0x3FF;
   while(ebda < ebdaEnd){
      u64 signature = *(u64 *)ebda;
      if(signature == *(const u64 *)"RSD PTR ")
      {
         if(parseRSDP(ebda) == 0)
	 {
	    return 0;
	 }
      }
   }

   printkInColor(0xff,0x00,0x00,"Can't find ACPI.\n");
   return -1;
}
