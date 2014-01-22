#include <core/const.h>
#include <video/console.h>
#include <driver/driver.h>
#include <driver/pci.h>
#include <lib/string.h>
#include <cpu/io.h>
#include <interrupt/interrupt.h>
#include <task/task.h>
#include <task/semaphore.h>
#include <filesystem/block.h>
#include <memory/buddy.h>

typedef enum IDEDeviceType{
   InvalidIDEDevice,
   IDEDeviceTypeATA,
   IDEDeviceTypeATAPI
} IDEDeviceType;

typedef enum IDEDeviceSubType{
   IDEDeviceSubTypeCDROM, /*CD*/
   IDEDeviceSubTypeDisk
} IDEDeviceSubType;

typedef struct IDEPort{
   u16 base,ctrl;
   u16 busMasterIDE;
   u8 irq;
} IDEPort;

typedef struct IDEDevice{
   IDEDeviceType type;
   IDEDeviceSubType subType;

   PhysicsPage *prdt;
   u8 dma;
   u8 primary,master;
   /*Primary is if this device is primary.*/
   /*Master is if this device is master.*/
   BlockDevice block;
} IDEDevice;

typedef struct IDEInterruptWait{
   u8 primary;
   Task *task;
   u8 status;
   u8 bmstatus;
} IDEInterruptWait;

/*IDE in PCI.*/
#define IDE_PCI_CLASS                0x01010000
#define IDE_PCI_CLASS_MASK           0xffff0000

/*IDE primary or secondary.*/
#define IDE_PRIMARY                  0x0
#define IDE_SECONDARY                0x1

/*IDE registers.*/
#define IDE_REG_DATA                 0x00
#define IDE_REG_FEATURES             0x01
#define IDE_REG_LBA0                 0x03
#define IDE_REG_LBA1                 0x04
#define IDE_REG_LBA2                 0x05
#define IDE_REG_DEV_SEL              0x06
#define IDE_REG_STATUS               0x07
#define IDE_REG_COMMAND              0x07
#define IDE_REG_BMCOMMAND            0x08
#define IDE_REG_BMSTATUS             0x0a
#define IDE_REG_BMPRDT               0x0c
#define IDE_REG_CONTROL              0xff

/*ATA commands.*/
#define IDE_CMD_IDENTIFY             0xec

/*ATAPI commands.*/
#define IDE_CMD_PACKET               0xa0
#define IDE_CMD_IDENTIFY_PACKET      0xa1

/*BM commands.*/
#define IDE_BMCMD_START              0x01
#define IDE_BMCMD_READ               0x08
#define IDE_BMCMD_WRITE              0x00

/*IDE cntrol register.*/
#define IDE_DISABLE_IRQ              0x02
#define IDE_ENABLE_IRQ               0x00

/*IDE status register.*/
#define IDE_STATUS_BUSY              0x80
#define IDE_STATUS_DRQ               0x08
#define IDE_STATUS_ERROR             0x01

#define ATAPI_SECTOR_SIZE            0x800 /*2048.*/

/*The IRQ vector of IDE.*/
#define IDE_PRIMARY_IRQ              14
#define IDE_SECONDARY_IRQ            15

/*Bus Master IDE status register.*/
#define IDE_BMSTATUS_INTERRUPT       0x04
#define IDE_BMSTATUS_ERROR           0x02
#define IDE_BMSTATUS_ACTIVE          0x01

static int ideDisable(Device *device);
static int ideProbe(Device *device);
static int ideEnable(Device *device);
static int cdromTask(void *arg);
static int ideIRQPrimary(IRQRegisters *reg,void *data);
static int ideIRQSecondary(IRQRegisters *reg,void *data);

                         /*Is primary?*/
static inline int ideOutb(u8 device,u8 reg,u8 data) __attribute__ ((always_inline));
static inline int ideOutl(u8 device,u8 reg,u32 data) __attribute__ ((always_inline));
static inline u8 ideInb(u8 device,u8 reg) __attribute__ ((always_inline));
static inline u32 ideInl(u8 device,u8 reg) __attribute__ ((always_inline));
static inline int ideInsl(u8 device,u8 reg,u64 size,void *buf) 
   __attribute__ ((always_inline));
static inline int ideInsw(u8 device,u8 reg,u64 size,void *buf) 
   __attribute__ ((always_inline));
static inline int ideOutsw(u8 device,u8 reg,u64 size,void *buf) 
   __attribute__ ((always_inline));

static IDEPort ports[2] = {{0},{0}};

static Semaphore ideSemaphores[2] = {};

static IDEDevice ideDevices[2][2] = {{{0},{0}},{{0},{0}}};

static Driver ideDriver = {
   .probe = &ideProbe,
   .disable = &ideDisable,
   .enable = &ideEnable
};

static inline int ideOutb(u8 device,u8 reg,u8 data)
{
   IDEPort *port = &ports[device];

   if(reg < 0x08)
      outb(port->base + reg,data);
   else if(reg < 0x10)
      outb(port->busMasterIDE + reg - 0x08,data);
   else
      outb(port->ctrl,data);
          /*Cntrol regsiter.*/

   return 0;
}

static inline int ideOutl(u8 device,u8 reg,u32 data)
{
   IDEPort *port = &ports[device];

   if(reg < 0x08)
      outl(port->base + reg,data);
   else if(reg < 0x10)
      outl(port->busMasterIDE + reg - 0x08,data);
   else
      outl(port->ctrl,data);

   return 0;
}

static inline int ideOutsw(u8 device,u8 reg,u64 size,void *buf)
{
   IDEPort *port = &ports[device];
   
   if(reg < 0x8)
      outsw(port->base + reg,size,buf);
   else if(reg < 0x10)
      outsw(port->busMasterIDE + reg - 0x08,size,buf);
   else
      outsw(port->ctrl,size,buf);

   return 0;

}

static inline u8 ideInb(u8 device,u8 reg)
{
   IDEPort *port = &ports[device];
   u8 ret;

   if(reg < 0x08)
      ret = inb(port->base + reg);
   else if(reg < 0x10)
      ret = inb(port->busMasterIDE + reg - 0x08);
   else
      ret = inb(port->ctrl);

   return ret;
}

static inline u32 ideInl(u8 device,u8 reg)
{
   IDEPort *port = &ports[device];
   u32 ret;

   if(reg < 0x08)
      ret = inl(port->base + reg);
   else if(reg < 0x10)
      ret = inl(port->busMasterIDE + reg - 0x08);
   else
      ret = inl(port->ctrl);

   return ret;
}

static inline int ideInsl(u8 device,u8 reg,u64 size,void *buf)
{
   IDEPort *port = &ports[device];
  
   if(reg < 0x8)
      insl(port->base + reg,size,buf);
   else if(reg < 0x10)
      insl(port->busMasterIDE + reg - 0x08,size,buf);
   else
      insl(port->ctrl,size,buf);

   return 0;
}

static inline int ideInsw(u8 device,u8 reg,u64 size,void *buf)
{
   IDEPort *port = &ports[device];

   if(reg < 0x8)
      insw(port->base + reg,size,buf);
   else if(reg < 0x10)
      insw(port->busMasterIDE + reg - 0x08,size,buf);
   else
      insw(port->ctrl,size,buf);

   return 0;
}

static int ideWaitReady(u8 device)
{
   u8 status;
   
   for(;;){
      status = ideInb(device,IDE_REG_STATUS);
      if(!(status & IDE_STATUS_BUSY))
         break;
      schedule();
   }

   return status & IDE_STATUS_ERROR;
}

static int ideWaitDRQ(u8 device)
{
   u8 error = 0;
   for(;;)
   {
      u8 status = ideInb(device,IDE_REG_STATUS);
      if(status & IDE_STATUS_ERROR) /*Error?*/
      {
         error = 1;
         break;
      }
      if((!(status & IDE_STATUS_BUSY)) && (status & IDE_STATUS_DRQ))
         break;
      schedule(); /*Give time to other tasks.*/
   }
   return error;
}

static int ideSendCommandATAPI(IDEDevice *device,u8 *cmd,
                                 u16 cmdSize,u16 transSize,void *buf,u8 dma)
{
   int sizeRead = transSize;
   Task *current = getCurrentTask();
   const u8 primary = device->primary;
   const u8 master = device->master;
   IDEInterruptWait wait = {.task = current,.primary = primary};
   u8 irq = 
      (primary == IDE_PRIMARY) ? IDE_PRIMARY_IRQ : IDE_SECONDARY_IRQ;

   dma &= device->dma;
   
   downSemaphore(&ideSemaphores[primary]);

   /*Setup DMA.*/
   ideOutb(primary,IDE_REG_CONTROL,IDE_ENABLE_IRQ);
   if(dma)
   {
      u64 *prdt = getPhysicsPageAddress(device->prdt);
      pointer address = va2pa(buf);
      u64 data = (u32)address;
      data |= ((u64)transSize) << 32;
      data |= 0x8000000000000000ul;
      *prdt = data; /*Set the PRD Table.*/

      address = va2pa(prdt); /*Get the physics address of PRDT.*/
      ideOutb(primary,IDE_REG_BMCOMMAND,0); /*Stop!*/
      ideOutb(primary,IDE_REG_BMSTATUS,ideInb(primary,IDE_REG_BMSTATUS)); 
               /*Clear all bits of the Status Register.*/
      ideOutl(primary,IDE_REG_BMPRDT,address); /*Set the PRDT Register.*/
      ideOutb(primary,IDE_REG_BMCOMMAND,IDE_BMCMD_READ); 
      ideOutb(primary,IDE_REG_BMCOMMAND,IDE_BMCMD_START | IDE_BMCMD_READ);
                /*Start the DMA transfer.*/
  }

   ideWaitReady(primary);
   ideOutb(primary,IDE_REG_DEV_SEL,(master << 4) | 0xa0);
   ideWaitReady(primary);
   if(!dma)
   {
      ideOutb(primary,IDE_REG_FEATURES,0x0); /*PIO Mode.*/
      ideOutb(primary,IDE_REG_LBA1,(transSize & 0xff));
      ideOutb(primary,IDE_REG_LBA2,(transSize >> 8) & 0xff);
   }else{
      ideOutb(primary,IDE_REG_FEATURES,0x1); /*DMA Mode.*/
      ideOutb(primary,IDE_REG_LBA1,0);
      ideOutb(primary,IDE_REG_LBA2,0);
   }
   ideOutb(primary,IDE_REG_COMMAND,IDE_CMD_PACKET);/*Send command.*/

   if(ideWaitDRQ(primary))
      goto failed;

   current->state = TaskStopping;
   setIRQData(irq,&wait);
   ideOutsw(primary,IDE_REG_DATA,cmdSize / 2,cmd);
   schedule(); /*Wait Interrupt.*/
   setIRQData(irq,0);

   if(dma && (wait.bmstatus & IDE_BMSTATUS_ERROR))
      goto failed;  /*Error!*/
   else if(dma && (wait.bmstatus & IDE_BMSTATUS_ACTIVE))
      goto failed; /*Eorror!*/
   else if(dma)
      goto out; /*DMA transfer done.*/

   if(ideWaitDRQ(primary))
      goto failed;
   sizeRead = ideInb(primary,IDE_REG_LBA1);
   sizeRead |= ideInb(primary,IDE_REG_LBA2) << 8;

   current->state = TaskStopping;
   setIRQData(irq,&wait);
   if(buf)
      ideInsl(primary,IDE_REG_DATA,sizeRead / 4,buf);
   else
      while((sizeRead -= 4) >= 0)
         ideInl(primary,IDE_REG_DATA);
   schedule();
   setIRQData(irq,&wait);

out:
   upSemaphore(&ideSemaphores[primary]);
   return (sizeRead == transSize) ? 0 : -EIO;
      /*Failed if sizeRead != transSize.*/
failed:
   upSemaphore(&ideSemaphores[primary]);
   return -EIO;
}

static int ideGetDMASupportATAPI(IDEDevice *device)
{
   u8 cmd[12] = {0xa8,0,0,0,0,0,0,0,0,1,0,0};
   u8 *buf = (u8 *)allocDMAPages(0,32); /*Alloc a DMA buffer.*/
   if(!buf)
      return -ENOMEM;
   buf = (u8 *)getPhysicsPageAddress((PhysicsPage *)buf);
   memset(buf,0x11,ATAPI_SECTOR_SIZE);
   u8 retval;

   device->dma = 1;
   for(;;)
   {
      retval =
         ideSendCommandATAPI(device,cmd,sizeof(cmd),ATAPI_SECTOR_SIZE,buf,1); 
      if(retval && (device->dma = 0,1)) /*Try to read data using DMA.*/
         goto out;
      for(int i = 0;i < ATAPI_SECTOR_SIZE;i += sizeof(u64))
         if(*(u64 *)&buf[i] != 0x1111111111111111ul)
            goto out; /*If the buffer is changed,DMA is supported.*/
      retval = 
         ideSendCommandATAPI(device,cmd,sizeof(cmd),ATAPI_SECTOR_SIZE,buf,0); 
                                         /*Try to read data using PIO.*/
      if(retval && (device->dma = 0,1))
         goto out;
      for(int i = 0;i < ATAPI_SECTOR_SIZE;i += sizeof(u64))
         if((*(u64 *)&buf[i] != 0x1111111111111111ul) && (device->dma = 0,1))
            goto out; /*If the buffer is changed,DMA is not supported.*/
      if(++cmd[5] == 0)
         if(++cmd[4] == 0)
            if(++cmd[3] == 0)
               if((++cmd[2] == 0) && (device->dma = 0,1))
                  goto out; /*Try to read the next sector.*/
   }
out:
   freePages(getPhysicsPage(buf),0); /*Free the buffer.*/
   return retval;
}

static int ideReadSectorATAPI(IDEDevice *device,u64 lba,u8 sector,void *buf)
{
   u8 cmd[12] = {0xa8 /*READ (12).*/,0,0,0,0,0,0,0,0,0,0,0};
      /*SCSI Command.*/
   if(sector >= 32)
      return -EINVAL;

   cmd[9] = sector;
   cmd[2] = (lba >> 0x18) & 0xff;
   cmd[3] = (lba >> 0x10) & 0xff;
   cmd[4] = (lba >> 0x08) & 0xff;
   cmd[5] = (lba >> 0x00) & 0xff; /*Set this scsi command.*/

   int ret = ideSendCommandATAPI(device,cmd,sizeof(cmd),sector * ATAPI_SECTOR_SIZE,buf,1);

   return ret;
}

static int ideRead(void *data,u64 start,u64 size,void *buf)
{
   IDEDevice *device = (IDEDevice *)data;
   if(device->type == IDEDeviceTypeATA)
   {
      /*We will support it in the future.*/
      return -ENOSYS;
   }else if(device->type == IDEDeviceTypeATAPI)
   {
      int ret = -EIO;
      void *__buf = allocDMAPages(0,32);
      if(!__buf) /*Alloc a DMA buffer at first.*/
         return -ENOMEM;
      __buf = getPhysicsPageAddress((PhysicsPage *)__buf);
      u64 lba = start / ATAPI_SECTOR_SIZE;
      if(start % ATAPI_SECTOR_SIZE != 0)
      {
         if(ideReadSectorATAPI(device,lba,1,__buf))
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
         if(ideReadSectorATAPI(device,lba,PAGE_SECTOR,__buf))
            goto out;
         memcpy((void *)buf,(const void *)__buf,PHYSICS_PAGE_SIZE);
         buf += PHYSICS_PAGE_SIZE;
         lba += PAGE_SECTOR;
         count -= PAGE_SECTOR;
      }
#undef PAGE_SECTOR
      if(!count)
         goto next;
      if(ideReadSectorATAPI(device,lba,count,__buf))
         goto out;
      memcpy((void *)buf,(const void *)__buf,count * ATAPI_SECTOR_SIZE);
      buf += ATAPI_SECTOR_SIZE * count;
next:
      if(size % ATAPI_SECTOR_SIZE != 0)
      {
         if(ideReadSectorATAPI(device,lba,1,__buf))
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

static int ideParseIdentifyData(IDEDevice *device,IDEDeviceType type,void *__data)
{
   /*For more information about this,
    * see also:
    * Information Technology -
    * AT Attachment with Packet Interface - 7
    * Volume 1 - Register Delivered Command Set, Logical
    * Register Set
    * (ATA/ATAPI-7 V1)*/
   /*Page 148.*/

   u16 *data = (u16 *)__data;
   device->type = type;
   if(type == IDEDeviceTypeATA)
   {
      return -ENOSYS;
   }else if(type == IDEDeviceTypeATAPI)
   {
      u8 deviceType = ((*data) & 0x1f00) >> 8;
      if(deviceType != 0x5) 
         return -ENOSYS;  /*CD-ROM device?*/
      device->subType = IDEDeviceSubTypeCDROM;
      device->dma = !!(data[49] & (1 << 8));
      if(device->dma)
         ideGetDMASupportATAPI(device);
   }else
   {
      return -ENOSYS;
   }
   return 0;
}

static int ideProbe(Device *device)
{
   if(ports[0].base)
      return 1; /*It has been inited.*/
   if(device->type != DeviceTypePCI)
      return 1; /*Is it PCI Device?*/
   PCIDevice *pci = containerOf(device,PCIDevice,globalDevice);
   if((pci->class & IDE_PCI_CLASS_MASK) != IDE_PCI_CLASS)
      return 1; /*Is it IDE?*/

   return 0;
}

static int ideEnable(Device *device)
{
   if(ports[0].base != 0)
      return -EBUSY;
   PCIDevice *pci = containerOf(device,PCIDevice,globalDevice);

   ports[IDE_PRIMARY  ].base = pci->bar[0] ? : 0x1f0;
   ports[IDE_PRIMARY  ].ctrl = pci->bar[1] ? : 0x3f4;
   ports[IDE_PRIMARY  ].busMasterIDE = (pci->bar[4] & 0xfffffffc) + 0;

   ports[IDE_SECONDARY].base = pci->bar[2] ? : 0x170;
   ports[IDE_SECONDARY].ctrl = pci->bar[3] ? : 0x374;
   ports[IDE_SECONDARY].busMasterIDE = (pci->bar[4] & 0xfffffffc) + 8;

   requestIRQ(IDE_PRIMARY_IRQ,&ideIRQPrimary);
   requestIRQ(IDE_SECONDARY_IRQ,&ideIRQSecondary); 
      /*Request these IRQs.*/

   u8 buf[128 * 4];

   for(int i = 0;i < 2;++i)
   {
      for(int j = 0;j < 2;++j)
      {
         ideOutb(IDE_PRIMARY  ,IDE_REG_CONTROL,IDE_DISABLE_IRQ);
         ideOutb(IDE_SECONDARY,IDE_REG_CONTROL,IDE_DISABLE_IRQ);

         ideDevices[i][j].primary = i;
         ideDevices[i][j].master = j;
         ideDevices[i][j].type = InvalidIDEDevice;
         ideDevices[i][j].prdt = 0;

         u8 error = 0;
         IDEDeviceType type = IDEDeviceTypeATA;
         ideOutb(i,IDE_REG_DEV_SEL,0xa0 | (j << 4));
            /*Select master.*/
         for(int k = 0;k < 4;++k) /*A little delay.*/
            ideInb(i,IDE_REG_CONTROL);

         ideOutb(i,IDE_REG_COMMAND,IDE_CMD_IDENTIFY);
         for(int k = 0;k < 4;++k) /*A little delay.*/
           ideInb(i,IDE_REG_CONTROL);

         if(!ideInb(i,IDE_REG_STATUS))
            continue;

         error = ideWaitDRQ(i);
         if(error)
         { /*It is not an ata device if error.*/
           /*Maybe it is an atapi device. (Such as CD-ROM.)*/
            u8 cl = ideInb(i,IDE_REG_LBA1);
            u8 ch = ideInb(i,IDE_REG_LBA2);

            if((cl == 0x14) && (ch == 0xeb))
               ;
            else if((cl == 0x69) && (ch == 0x96))
               ;
            else /*It is not an atapi device.*/
               continue;
            type = IDEDeviceTypeATAPI;
            ideOutb(i,IDE_REG_COMMAND,IDE_CMD_IDENTIFY_PACKET); 
            error = ideWaitDRQ(i); /*Wait DRQ again.*/
            if(error)
               continue;
         }

         ideInsl(i,IDE_REG_DATA,128,buf);
         
         ideDevices[i][j].prdt = allocDMAPages(0,32);
         ideParseIdentifyData(&ideDevices[i][j],type,buf);
            /*Read and parse it.*/
         if(ideDevices[i][j].subType == IDEDeviceSubTypeCDROM)
         {
            BlockDevice *block = &ideDevices[i][j].block;
            block->write = 0;
            block->read = &ideRead;
            block->type = BlockDeviceCDROM;
            block->data = (void *)&ideDevices[i][j];
            block->end = (u64)-1;
            registerBlockDevice(block,"cdrom");
            //createKernelTask(&cdromTask,&ideDevices[i][j]);
            (void)cdromTask;
         }
      }
   }
   return 0;
}

static int ideDisable(Device *device)
{
   for(int i = 0;i < 2;++i)
   {
      for(int j = 0;j < 2;++j)
      {
         if(ideDevices[i][j].prdt)
            freePages(ideDevices[i][j].prdt,0);
      }
   }
   memset(ports,0,sizeof(ports));
     /*Only memset it.*/
   freeIRQ(IDE_PRIMARY_IRQ);
   freeIRQ(IDE_SECONDARY_IRQ);
   return 0;
}

static int cdromStatusCheck(IDEDevice *device)
{
   u8 cmd[12] = {0x25 /*Read Capacity.*/,0,0,0,0,0,0,0,0,0,0,0};
   int ret = ideSendCommandATAPI(device,cmd,sizeof(cmd),8,0,0);
   if(ret < 0)
      return 1; /*Not inserted.*/
   return 0; /*This media is inserted.*/
}

static int cdromTask(void *arg)
{
   IDEDevice *device = (IDEDevice *)arg;
   int status = cdromStatusCheck(device);
   int newStatus;
   for(;;)
   {
      scheduleTimeout(1000); /*1 second.*/
      newStatus = cdromStatusCheck(device);
      if(status == newStatus)
         continue; /*Status is changed?*/
      status = newStatus;
      switch(status) /*Print some information.*/
      {
      case 0:
         printk("CDROM inserted!!\n");
         break;
      case 1:
         printk("CDROM ejected!!\n");
         break;
      default:
         break;
      }
   }
   return 0;
}

static int ideIRQCommon(int primary,IDEInterruptWait *wait)
{
   if(wait)
      if(wait->primary != primary)
         return -EINVAL; /*It should never arrive here.*/
   u8 busMasterIDEStatus = ideInb(primary,IDE_REG_BMSTATUS);
   u8 status = ideInb(primary,IDE_REG_STATUS); /*Get status.*/
   if(!(busMasterIDEStatus & IDE_BMSTATUS_INTERRUPT))
      return -ENODEV; 
   ideOutb(primary,IDE_REG_BMSTATUS,IDE_BMSTATUS_INTERRUPT);
   ideOutb(primary,IDE_REG_BMCOMMAND,0x0);

   if(wait)
   {
      wait->bmstatus = busMasterIDEStatus;
      wait->status = status; /*Wake up the task that is waiting.*/
      wakeUpTask(wait->task);
   }
   return 0;
}

static int ideIRQPrimary(IRQRegisters *reg,void *data)
{
   ideIRQCommon(IDE_PRIMARY,(IDEInterruptWait *)data);
   return 0; /*Just call ideIRQCommon.*/
}

static int ideIRQSecondary(IRQRegisters *reg,void *data)
{
   ideIRQCommon(IDE_SECONDARY,(IDEInterruptWait *)data);
   return 0;
}

static int initIDE(void)
{
   for(int i = 0;i < sizeof(ideSemaphores) / sizeof(ideSemaphores[0]);++i)
      initSemaphore(&ideSemaphores[i]);
   registerDriver(&ideDriver);
   return 0;
}

driverInitcall(initIDE);
