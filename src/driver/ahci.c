#include <core/const.h>
#include <driver/driver.h>
#include <driver/pci.h>
#include <memory/paging.h>
#include <memory/buddy.h>
#include <video/console.h>
#include <block/block.h>
#include <interrupt/interrupt.h>
#include <task/task.h>
#include <task/semaphore.h>
#include <lib/string.h>

typedef volatile struct AHCIPort{
   u64 commandList;
   u64 fis;
   u32 interruptStatus;
   u32 interruptEnable;
   u32 command; /*Command And Status.*/
   u32 reserved0;
   u32 taskFile;
   u32 signature;
   u32 status;
   u32 control;
   u32 error;
   u32 active;
   u32 issue;
   u32 notification;
   u32 fisControl;
   u8 reserved1[0x70 - 0x44];
   u8 vendor[0x80 - 0x70];
} __attribute__ ((packed)) AHCIPort;

typedef volatile struct AHCIRegisters {
   u32 capability;
   u32 hcontrol;
   u32 interrupt;
   u32 port;   /*Port Implemented.*/
   u32 version;
   u32 ccontrol;
   u32 cports;
   u32 elocation;
   u32 econtrol;
   u32 ecapability;
   u32 status;

   u8 reserved[0xa0 - 0x2c];
   u8 vendor[0x100 - 0xa0];
   
   AHCIPort ports[0];
} __attribute__ ((packed)) AHCIRegisters;

typedef struct AHCICommandHeader{
   u8 length:5;
   u8 atapi:1;
   u8 write:1;
   u8 prefetchable:1;

   u8 reset:1;
   u8 bist:1;
   u8 busy:1;
   u8 reserved0:1;
   u8 multiplier:4;

   u16 prdtl;  /*Physical region descriptor table length in entries.*/
   u32 prdbc;  /*Physical region descriptor byte count transferred.*/

   u64 command; /*The address of AHCICommandTable.*/

   u32 reserved1[4];
} __attribute__ ((packed)) AHCICommandHeader;

typedef struct AHCIPrd{
   u64 address;
   u32 reserved0;

   u32 count:22;
   u32 reserved1:9;
   u32 interrupt:1;
} __attribute__ ((packed)) AHCIPrd;

typedef struct AHCICommandTable{
   u8 fis[64];
   u8 scsi[16];   /*A SCSI Command. (For ATAPI Devices.)*/
   u8 reserved[48];
   
   AHCIPrd prdt[0];
} __attribute__ ((packed)) AHCICommandTable;

typedef struct AHCIHostToDeviceFIS{
   u8 type;
   
   u8 multiplier:4;
   u8 reserved0:3;
   u8 cc:1; /*0: Control,1: Command.*/

   u8 command;
   u8 featurel;

   u8 lba0;
   u8 lba1;
   u8 lba2;
   u8 device;

   u8 lba3;
   u8 lba4;
   u8 lba5;
   u8 featureh;

   u16 count;
   u8 completion;
   u8 control;

   u8 reserved1[4];
} __attribute__ ((packed)) AHCIHostToDeviceFIS;

#define AHCI_PCI_CLASS          0x01060000
#define AHCI_PCI_CLASS_MASK     0xffff0000

#define AHCI_MAX_PORTS          32

#define AHCI_PORT_ATA           0x00000101
#define AHCI_PORT_ATAPI         0xeb140101

#define AHCI_FIS_D2H            0x27
#define AHCI_FIS_H2D            0x34

#define AHCI_GHC_ENABLE_IRQ     0x2
#define AHCI_GHC_RESET_HBA      0x1

#define AHCI_PORT_ENABLE_IRQ    0x1

#define AHCI_PORT_DET_MASK      0x000f
#define AHCI_PORT_IPM_MASK      0x0f00
#define AHCI_PORT_DET_PRESENT   0x0003
#define AHCI_PORT_IPM_ACTIVE    0x0100

#define AHCI_COMMAND_START      0x00000001
#define AHCI_COMMAND_FRE        0x00000010
#define AHCI_COMMAND_FR         0x00008000
#define AHCI_COMMAND_CR         0x00004000

#define ATAPI_SECTOR_SIZE       0x800

static int ahciProbe(Device *device);
static int ahciEnable(Device *device);
static int ahciDisable(Device *device);

static AHCIRegisters *ahciHBA;
static u8 ahciIRQVector;
static BlockDevice ahciBlockDevices[AHCI_MAX_PORTS];
static Semaphore ahciSemaphore;

static Driver ahciDriver = 
{
   .probe = &ahciProbe,
   .enable = &ahciEnable,
   .disable = &ahciDisable
};

static int ahciStopCommand(AHCIPort *port)
{
   port->command &= ~AHCI_COMMAND_START;
   return 0;
}

static int ahciStartCommand(AHCIPort *port)
{
   port->command |= AHCI_COMMAND_FRE;
   port->command |= AHCI_COMMAND_START;
   return 0;
}

static int ahciSendCommandATAPI(AHCIPort *port,u8 *command,u8 commandSize,
                               void *buffer,u64 transferSize)
{
   if(commandSize != 12 && commandSize != 16)
      return -EINVAL;
   downSemaphore(&ahciSemaphore);

   AHCICommandHeader *header = pa2va(port->commandList);
   AHCICommandTable *table = pa2va(header->command);
   AHCIHostToDeviceFIS *fis = (AHCIHostToDeviceFIS *)table->fis;
   u64 __command = header->command;  /*Store the address.*/
   Task *current = getCurrentTask();
   void *data[] = {current,(void *)port};

   memset(header,0,sizeof(*header));
   header->command = __command; /*Restore the address.*/
   header->length = sizeof(*fis) / sizeof(u32);
   header->atapi = 1;
   header->write = 0;  /*Read.*/
   header->prdtl = 1;
   
   memset(table,0,sizeof(*table) + header->prdtl * sizeof(AHCIPrd));
   memcpy((void *)table->scsi,(const void *)command,commandSize);

   fis->type = AHCI_FIS_H2D; /*Host To Device.*/
   fis->cc = 1; /*Command.*/
   fis->command = 0xa0; /*Packet Command.*/

   table->prdt[0].interrupt = 1;
   table->prdt[0].address = va2pa(buffer);
   table->prdt[0].count = transferSize;

   ahciStartCommand(port);

   current->state = TaskUninterruptible;
   setIRQData(ahciIRQVector,data);
   port->issue = 1;
   schedule(); /*Wait for an interrupt.*/
   setIRQData(ahciIRQVector,0);

   ahciStopCommand(port);

   upSemaphore(&ahciSemaphore);
   return (header->prdbc == transferSize) ? 0 : -EIO;
}

static int ahciReadSectorATAPI(AHCIPort *port,u64 lba,u8 sector,void *buffer)
{
   u8 cmd[12] = {0xa8,0,0,0,0,0,0,0,0,0,0,0};
   if(sector >= 32)
      return -EINVAL;
   cmd[9] = sector;
   cmd[2] = (lba >> 0x18) & 0xff;
   cmd[3] = (lba >> 0x10) & 0xff;
   cmd[4] = (lba >> 0x08) & 0xff;
   cmd[5] = (lba >> 0x00) & 0xff; /*Set this scsi command.*/

   return ahciSendCommandATAPI(port,cmd,sizeof(cmd),buffer,sector * ATAPI_SECTOR_SIZE);
      /*Send the command.*/
}

static int ahciRead(void *data,u64 start,u64 size,void *buf)
{
   AHCIPort *port = (AHCIPort *)data;
   if(port->signature == AHCI_PORT_ATA)
   {
      /*We will support it in the future.*/
      return -ENOSYS;
   }else if(port->signature == AHCI_PORT_ATAPI)
   {
      int ret = -EIO;
      void *__buf = allocPages(0); /*Alloc a DMA buffer at first.*/
      /*AHCI Support 64-bit physics address.So we just call 'allocPages'.*/
      if(!__buf) 
         return -ENOMEM; 
      __buf = getPhysicsPageAddress((PhysicsPage *)__buf);
      u64 lba = start / ATAPI_SECTOR_SIZE;
      if(start % ATAPI_SECTOR_SIZE != 0)
      {
         if(ahciReadSectorATAPI(port,lba,1,__buf))
            goto out;
         u64 __size;
         if(size < ATAPI_SECTOR_SIZE - start % ATAPI_SECTOR_SIZE)
            __size = size;
         else
            __size = ATAPI_SECTOR_SIZE - start % ATAPI_SECTOR_SIZE;
         memcpy((void *)buf,(const void *)__buf + start % ATAPI_SECTOR_SIZE,__size);
         ++lba;
         size -= __size;
         ret = 0;
         if(size == 0)
            goto out;
         ret = -EIO;
      }
      int count = size / ATAPI_SECTOR_SIZE;
      if(start % ATAPI_SECTOR_SIZE != 0)
         --count;
#define PAGE_SECTOR (PHYSICS_PAGE_SIZE / ATAPI_SECTOR_SIZE)
      while(count > PAGE_SECTOR)
      {
         if(ahciReadSectorATAPI(port,lba,PAGE_SECTOR,__buf))
            goto out;
         memcpy((void *)buf,(const void *)__buf,PHYSICS_PAGE_SIZE);
         buf += PHYSICS_PAGE_SIZE;
         lba += PAGE_SECTOR;
         count -= PAGE_SECTOR;
      }
#undef PAGE_SECTOR
      if(!count)
         goto next;
      if(ahciReadSectorATAPI(port,lba,count,__buf))
         goto out;
      memcpy((void *)buf,(const void *)__buf,count * ATAPI_SECTOR_SIZE);
      buf += ATAPI_SECTOR_SIZE * count;
next:
      if(size % ATAPI_SECTOR_SIZE != 0)
      {
         if(ahciReadSectorATAPI(port,lba,1,__buf))
            goto out;
         memcpy(buf,__buf,size % ATAPI_SECTOR_SIZE);
      }
      ret = 0;
out:
      freePages(getPhysicsPage(__buf),0); /*Free the buffer.*/
      return ret;
   }else
   {
      return -ENOSYS;
   }
}

static int ahciProbe(Device *device)
{
   if(device->type != DeviceTypePCI)
      return 1;
   PCIDevice *pci = containerOf(device,PCIDevice,globalDevice);
   if((pci->class & AHCI_PCI_CLASS_MASK) != AHCI_PCI_CLASS)
      return 1;
   return 0;
}

static int ahciIRQ(IRQRegisters *reg,void *__data)
{
   if(!__data)
      return 0;
   if(!ahciHBA->interrupt)
      return 0;
   ahciHBA->interrupt = ahciHBA->interrupt; /*Clear all bits.*/
   void **data = (void **)__data;
   AHCIPort *port = (AHCIPort *)data[1];
   if(!port->interruptStatus)
      return 0;
   port->interruptStatus = port->interruptStatus; /*Clear all bits.*/
   wakeUpTask((Task *)data[0],0); /*Wake up the task.*/
   return 0;
}

static int ahciDisablePortATAPI(AHCIPort *port,int i)
{
   /*if(ahciBlockDevices[i].read)*/
   /*   deregisterBlockDevice(&ahciBlockDevices[i])*/

   ahciStopCommand(port);
   port->interruptEnable = 0;

   void *data = pa2va(port->fis);
   freePages(getPhysicsPage(data),0);
   data = pa2va(port->commandList);
   AHCICommandHeader *header = (AHCICommandHeader *)data;
   freePages(getPhysicsPage(pa2va(header->command)),0);
   freePages(getPhysicsPage(data),0); /*Free these pages.*/
   return 0;
}

static int ahciEnablePortATAPI(AHCIPort *port,int i)
{
   {
      PhysicsPage *cmd = allocPages(0);
      if(!cmd)
         return -ENOMEM;
      PhysicsPage *fis = allocPages(0);
      if(!fis)
         return (freePages(cmd,0),-ENOMEM);
      PhysicsPage *table = allocPages(0); /*Alloc some pages.*/
      if(!table)
         return (freePages(cmd,0),freePages(fis,0),-ENOMEM);

      port->fis = va2pa(getPhysicsPageAddress(fis));
      port->commandList = va2pa(getPhysicsPageAddress(cmd));
      port->interruptEnable = AHCI_PORT_ENABLE_IRQ;  /*Enable interrupts.*/
      port->interruptStatus = 0;

      AHCICommandHeader *header = (AHCICommandHeader *)getPhysicsPageAddress(cmd);
      header->command = va2pa(getPhysicsPageAddress(table));
   }

   {
      BlockDevice *block = &ahciBlockDevices[i];
      block->read = 0; /*This port has not been registered.*/

     /*Now we are going to send a PACKET IDENTIFY Command.*/
      u8 *buffer = (u8 *)allocPages(0);
      if(!buffer)
         return ahciDisablePortATAPI(port,i); /*Alloc a buffer.*/
      buffer = (u8 *)getPhysicsPageAddress((PhysicsPage *)buffer);

      AHCICommandHeader *header = pa2va(port->commandList);
      AHCICommandTable *table = pa2va(header->command);
      AHCIHostToDeviceFIS *fis = (AHCIHostToDeviceFIS *)table->fis;
      u64 __command = header->command;  /*Store the address.*/
      Task *current = getCurrentTask();
      void *data[] = {current,(void *)port};

      memset(header,0,sizeof(*header));
      header->command = __command; /*Restore the address.*/
      header->length = sizeof(*fis) / sizeof(u32);
      header->atapi = 0;
      header->write = 0;  /*Read.*/
      header->prdtl = 1;
   
      memset(table,0,sizeof(*table) + header->prdtl * sizeof(AHCIPrd));

      fis->type = AHCI_FIS_H2D; /*Host To Device.*/
      fis->cc = 1; /*Command.*/
      fis->command = 0xa1; /*PACKET IDENTIFY Command.*/

      table->prdt[0].interrupt = 1;
      table->prdt[0].address = va2pa(buffer);
      table->prdt[0].count = 128 * 4;

      ahciStartCommand(port);

      current->state = TaskUninterruptible;
      setIRQData(ahciIRQVector,data);
      port->issue = 1; /*Send the command.*/
      schedule(); /*Wait for an interrupt.*/
      setIRQData(ahciIRQVector,0);

      ahciStopCommand(port);
     
      switch((((u16 *)buffer)[0] & 0x1f00) >> 8)
      {
      case 0x5: /*CD-ROM.*/
         block->write = 0;
         block->read = &ahciRead;
         block->type = BlockDeviceCDROM;
         block->data = (void *)port;
         block->end = (u64)-1;

         registerBlockDevice(block,"cdrom"); /*Register the block device.*/
         break;
      default: /*Not CD-ROM,no support for it.*/
         ahciDisablePortATAPI(port,i);
         break;
      }

      freePages(getPhysicsPage(buffer),0); /*Free the buffer.*/
   }
   
   return 0;
}

static int ahciEnable(Device *device)
{
   if(ahciHBA)
      return 0;
   PCIDevice *pci = containerOf(device,PCIDevice,globalDevice);
   AHCIRegisters *ahci = pa2va(pci->bar[5]);

   ahciHBA = ahci;
   ahci->hcontrol = AHCI_GHC_ENABLE_IRQ;
   requestIRQ(pci->interrupt & 0xff,&ahciIRQ);
   ahciIRQVector = pci->interrupt & 0xff;

   for(int i = 0;i < AHCI_MAX_PORTS;++i)
   {
      if(ahci->port & (1 << i)) /*Exists?*/
      {
         AHCIPort *port = &ahci->ports[i];
         u32 status = port->status;
         if((status & AHCI_PORT_IPM_MASK) != AHCI_PORT_IPM_ACTIVE)
            continue; /*Present?*/
         if((status & AHCI_PORT_DET_MASK) != AHCI_PORT_DET_PRESENT)
            continue; /*Active?*/
         switch(port->signature)
         {
         case AHCI_PORT_ATA:
            break;
         case AHCI_PORT_ATAPI:
            ahciEnablePortATAPI(port,i);
            break;
         default:
            break;
         }
      }
   }
   return 0;
}

static int ahciDisable(Device *device)
{
   if(!ahciHBA)
      return 0;

   AHCIRegisters *ahci = ahciHBA;

   for(int i = 0;i < AHCI_MAX_PORTS;++i)
   {
      if(ahci->port & (1 << i))
      {
         AHCIPort *port = &ahci->ports[i];
         u32 status = port->status;
         if((status & AHCI_PORT_IPM_MASK) != AHCI_PORT_IPM_ACTIVE)
            continue;
         if((status & AHCI_PORT_DET_MASK) != AHCI_PORT_DET_PRESENT)
            continue;
         switch(port->signature)
         {
         case AHCI_PORT_ATA:
            break;
         case AHCI_PORT_ATAPI:
            ahciDisablePortATAPI(port,i); /*Disable it!*/
            break;
         default:
            break;
         }
      }
   }

   ahci->hcontrol &= ~AHCI_GHC_ENABLE_IRQ; /*Disable interrupts.*/
   memset(ahciBlockDevices,0,sizeof(ahciBlockDevices));
   freeIRQ(ahciIRQVector);
   ahciHBA = 0;
   return 0;
}

static int initAHCI(void)
{
   ahciHBA = 0;
   memset(ahciBlockDevices,0,sizeof(ahciBlockDevices));
   initSemaphore(&ahciSemaphore);
   registerDriver(&ahciDriver); /*Register the driver.*/
   return 0;
}

driverInitcall(initAHCI);
