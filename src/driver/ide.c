#include <core/const.h>
#include <video/console.h>
#include <driver/driver.h>
#include <driver/pci.h>
#include <lib/string.h>
#include <cpu/io.h>
#include <interrupt/interrupt.h>
#include <task/task.h>

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

   u8 primary,master;
   /*Primary is if this device is primary.*/
   /*Master is if this device is master.*/
} IDEDevice;

typedef struct IDEInterruptWait{
   u8 primary;
   Task *task;
   u8 status;
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
#define IDE_REG_CONTROL              0xff

/*ATA commands.*/
#define IDE_CMD_IDENTIFY             0xec

/*ATAPI commands.*/
#define IDE_CMD_PACKET               0xa0
#define IDE_CMD_IDENTIFY_PACKET      0xa1

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
#define IDE_BMSTATUS_INTERRUPT       0x4

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
static inline int ideInsl(u8 device,u8 reg,u64 size,void *buf) 
   __attribute__ ((always_inline));
static inline int ideInsw(u8 device,u8 reg,u64 size,void *buf) 
   __attribute__ ((always_inline));
static inline int ideOutsw(u8 device,u8 reg,u64 size,void *buf) 
   __attribute__ ((always_inline));

static IDEPort ports[2] = {{0},{0}};

static IDEDevice ideDevices[2][2] = {{{0},{0}},{{0},{0}}};

static u8 ideIOBuffer[2048] = {0};

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

   return 0;
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

static int ideWaitInterrupt(IDEInterruptWait *wait,IDEDevice *device)
{ /*When we called this function,preemption is disabled.*/
   u8 irq = 
      (device->primary == IDE_PRIMARY) ? IDE_PRIMARY_IRQ : IDE_SECONDARY_IRQ;
         /*IRQ vector of device.*/
   Task *current = getCurrentTask();
   wait->task = current;
   wait->primary = device->primary; /*Init wait.*/
   current->state = TaskStopping;
   setIRQData(irq,(void *)wait); /*Stop current task and set IRQ data.*/
   return 0;
}

static int ideWaitInterruptEnd(IDEDevice *device)
{
   u8 irq = 
      (device->primary == IDE_PRIMARY) ? IDE_PRIMARY_IRQ : IDE_SECONDARY_IRQ;
   setIRQData(irq,0);
   return 0;
}

static int ideReadATAPI(IDEDevice *device,u64 lba,u64 size,void *buf)
{
   IDEInterruptWait wait;
   const u8 primary = device->primary;
   const u8 master = device->master;
   u8 cmd[12] = {0xa8 /*READ (12).*/,0,0,0,0,0,0,0,0,0,0,0};
      /*SCSI Command.*/

   ideWaitReady(primary); /*Wait for this device ready.*/
   ideOutb(primary,IDE_REG_DEV_SEL,master << 4);
   for(int i = 0;i < 4;++i) /*A little delay.*/
     ideInb(primary,IDE_REG_CONTROL);
   ideOutb(primary,IDE_REG_CONTROL,IDE_ENABLE_IRQ); /*Enable IRQs.*/
   ideOutb(primary,IDE_REG_FEATURES,0x0); /*PIO Mode.*/
   ideOutb(primary,IDE_REG_LBA1,(ATAPI_SECTOR_SIZE & 0xff)); 
   ideOutb(primary,IDE_REG_LBA2,((ATAPI_SECTOR_SIZE >> 8)) & 0xff);
   ideOutb(primary,IDE_REG_COMMAND,IDE_CMD_PACKET); /*Send command.*/

   if(ideWaitDRQ(primary))
      return -1; /*Return if error.*/

   cmd[9] = 
      (size % ATAPI_SECTOR_SIZE) 
      ? (size / ATAPI_SECTOR_SIZE + 1) 
      : (size / ATAPI_SECTOR_SIZE);
   cmd[2] = (lba >> 0x18) & 0xff;
   cmd[3] = (lba >> 0x10) & 0xff;
   cmd[4] = (lba >> 0x08) & 0xff;
   cmd[5] = (lba >> 0x00) & 0xff; /*Set this scsi command.*/

   disablePreemption();
   ideWaitInterrupt(&wait,device);
   ideOutsw(primary,IDE_REG_DATA,6,cmd);
   enablePreemption();
   schedule();
             /*Write and wait for an IRQ.*/
   ideWaitInterruptEnd(device);

   if(ideWaitDRQ(primary))
      return -1;

   u16 sizeRead = ideInb(primary,IDE_REG_LBA1);
   sizeRead |= ideInb(primary,IDE_REG_LBA2) << 8;

   disablePreemption();
   ideWaitInterrupt(&wait,device); 
   ideInsw(primary,IDE_REG_DATA,sizeRead / 2,buf);
                   /*Read data.*/
   enablePreemption();
   schedule();  /*Wait for another IRQ.*/
   ideWaitInterruptEnd(device); 

   return 0;
}

static int ideRead(IDEDevice *device,u64 lba,u64 size,void *buf)
{
   if(device->type == IDEDeviceTypeATA)
   {
      /*We will support it in the future.*/
      return -1;
   }else if(device->type == IDEDeviceTypeATAPI)
   {
      return ideReadATAPI(device,lba,size,buf);
   }else
   {
      return -1;
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
      return -1;
   }else if(type == IDEDeviceTypeATAPI)
   {
      u8 deviceType = ((*data) & 0x1f00) >> 8;
      if(deviceType != 0x5) 
         return -1;  /*CD-ROM device?*/
      device->subType = IDEDeviceSubTypeCDROM;
   }else
   {
      return -1;
   }
   return 0;
}

static int parseISO9660FileSystemDir(
   IDEDevice *device,u64 lba,u8 *buf8,int depth)
{
   u64 offset = 0;
   u8 isDir = 0;
   u8 needRead = 0;
   if(ideRead(device,lba,ATAPI_SECTOR_SIZE,buf8))
      return -1;
   for(;;offset += buf8[offset + 0x0] /*Length of Directory Record.*/)
   {
      while(offset >= ATAPI_SECTOR_SIZE)
      {
         offset -= ATAPI_SECTOR_SIZE;
         ++lba;
         needRead = 1;
         /*Read again.*/
      }
      if(needRead)
         if(ideRead(device,lba,ATAPI_SECTOR_SIZE,buf8))
            return -1;
      needRead = 0;
      
      if(buf8[offset + 0x0] == 0x0) /*No more.*/
         break;
       
      /*Location of extent (LBA) in both-endian format.*/
      u64 fileLBA = *(u32 *)(buf8 + offset + 0x2);
      if(fileLBA <= lba)
        continue;
      int __depth = depth;
      while(__depth--)
         printk("--");

      isDir = buf8[offset + 25] & 0x2; /*Is it a dir?*/

      u64 filesize = *(u32 *)(buf8 + offset + 10);

      /*Length of file identifier (file name). 
       * This terminates with a ';' character 
       * followed by the file ID number in ASCII coded decimal ('1').*/
      u64 filenameLength = buf8[offset + 32];
      if(!isDir)
         filenameLength -= 2; /*Remove ';' and '1'.*/

      char filename[filenameLength + 1]; /*Add 1 for '\0'.*/
      memcpy((void *)filename,
         (const void *)(buf8 + offset + 33),
         filenameLength);

      if((!isDir) && (filename[0] == '_'))
         filename[0] = '.';
      if((!isDir) && (filename[filenameLength - 1] == '.'))
         filename[filenameLength - 1] = '\0';
      else
         filename[filenameLength] = '\0';

      /*To lower.*/
      for(u64 i = 0;i < filenameLength;++i)
         if((filename[i] <= 'Z') && (filename[i] >= 'A'))
            filename[i] -= 'A' - 'a';
      if(isDir)
      {
         printk("LBA:%d.",(int)fileLBA);
         printk("Dirname:%s\n",filename);
         parseISO9660FileSystemDir(device,fileLBA,buf8,depth + 1);
         ideRead(device,lba,ATAPI_SECTOR_SIZE,buf8);
         /*We must read again.*/
      }
      else
      {
         if(filesize != 0)
            printk("LBA:%d.",(int)fileLBA);
         else
            printk("Null file,no LBA.");
            /*LBA may not be right if this file is null!*/
         printk("Filename:%s\n",filename);
      }
   }


   return 0;
}

static int parseISO9660FileSystem(IDEDevice *device)
{
   /*See also http://wiki.osdev.org/ISO_9660.*/
   /*And http://en.wikipedia.org/wiki/ISO_9660.*/
   u8  *buf8  = (u8  *)ideIOBuffer;

   /*System Area (32,768 B)     Unused by ISO 9660*/
   /*32786 = 0x8000.*/
   u64 lba = (0x8000 / 0x800);

   for(;;++lba)
   {
      if(ideRead(device,lba,ATAPI_SECTOR_SIZE,buf8))
         return -1; /*It may not be inserted if error.*/
      /*Identifier is always "CD001".*/
      if(buf8[1] != 'C' ||
         buf8[2] != 'D' ||
         buf8[3] != '0' ||
         buf8[4] != '0' ||
         buf8[5] != '1' )
         return -1;
       if(buf8[0] == 0xff) /*Volume Descriptor Set Terminator.*/
         return -1;
       if(buf8[0] != 0x01 /*Primary Volume Descriptor.*/)
         continue;
       
       /*Directory entry for the root directory.*/
       if(buf8[156] != 0x22 /*Msut 34 bits.*/)
          return -1;

       /*Location of extent (LBA) in both-endian format.*/
       lba = *(u32 *)(buf8 + 156 + 2);
       break;
   }
   
   return parseISO9660FileSystemDir(device,lba,buf8,0);
}

static int ideProbe(Device *device)
{
   if(ports[0].base)
      return -1; /*It has been inited.*/
   if(device->type != DeviceTypePCI)
      return -1; /*Is it PCI Device?*/
   PCIDevice *pci = containerOf(device,PCIDevice,globalDevice);
   if((pci->class & IDE_PCI_CLASS_MASK) != IDE_PCI_CLASS)
      return -1; /*Is it IDE?*/

   return 0;
}

static int ideEnable(Device *device)
{
   if(ports[0].base != 0)
      return -1;
   PCIDevice *pci = containerOf(device,PCIDevice,globalDevice);

   ports[IDE_PRIMARY  ].base = pci->bar[0] ? : 0x1f0;
   ports[IDE_PRIMARY  ].ctrl = pci->bar[1] ? : 0x3f4;
   ports[IDE_PRIMARY  ].busMasterIDE = (pci->bar[4] & 0xfffffffc) + 0;

   ports[IDE_SECONDARY].base = pci->bar[2] ? : 0x170;
   ports[IDE_SECONDARY].ctrl = pci->bar[3] ? : 0x374;
   ports[IDE_SECONDARY].busMasterIDE = (pci->bar[4] & 0xfffffffc) + 8;

   ideOutb(IDE_PRIMARY  ,IDE_REG_CONTROL,IDE_DISABLE_IRQ);
   ideOutb(IDE_SECONDARY,IDE_REG_CONTROL,IDE_DISABLE_IRQ);

   for(int i = 0;i < 2;++i)
   {
      for(int j = 0;j < 2;++j)
      {
         ideDevices[i][j].primary = i;
         ideDevices[i][j].master = j;
         ideDevices[i][j].type = InvalidIDEDevice;

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

         ideInsl(i,IDE_REG_DATA,128,ideIOBuffer);
         
         ideParseIdentifyData(&ideDevices[i][j],type,ideIOBuffer);
            /*Read and parse it.*/
         if(ideDevices[i][j].subType == IDEDeviceSubTypeCDROM)
         {
            parseISO9660FileSystem(&ideDevices[i][j]);
                  /*Parse it!*/
            createKernelTask(&cdromTask,&ideDevices[i][j]);
         }
      }
   }
   requestIRQ(IDE_PRIMARY_IRQ,&ideIRQPrimary);
   requestIRQ(IDE_SECONDARY_IRQ,&ideIRQSecondary); 
      /*Request these IRQs.*/
   return 0;
}

static int ideDisable(Device *device)
{
   memset(ports,0,sizeof(ports));
     /*Only memset it.*/
   freeIRQ(IDE_PRIMARY_IRQ);
   freeIRQ(IDE_SECONDARY_IRQ);
   return 0;
}

static int cdromStatusCheck(IDEDevice *device)
{
   disablePreemption();

   IDEInterruptWait wait;
   const int primary = device->primary;
   const int master = device->master;
   u8 cmd[12] = {0x25 /*Read Capacity.*/,0,0,0,0,0,0,0,0,0,0,0};

   ideWaitReady(primary);
   ideOutb(primary,IDE_REG_DEV_SEL,master << 4);
   for(int i = 0;i < 4;++i) /*A little delay.*/
     ideInb(primary,IDE_REG_CONTROL);
   ideOutb(primary,IDE_REG_CONTROL,IDE_ENABLE_IRQ);
   ideOutb(primary,IDE_REG_FEATURES,0x0); /*PIO Mode.*/
   ideOutb(primary,IDE_REG_LBA1,(0x08 & 0xff)); /*8 Bytes.*/ 
   ideOutb(primary,IDE_REG_LBA2,((0x08 >> 8)) & 0xff);
   ideOutb(primary,IDE_REG_COMMAND,IDE_CMD_PACKET);

   if(ideWaitDRQ(primary))
      return -1; /*Return if error.*/

   ideWaitInterrupt(&wait,device);
   
   ideOutsw(primary,IDE_REG_DATA,6,cmd);
             /*Write and wait for an IRQ.*/
   schedule();
   ideWaitInterruptEnd(device);

   if(ideWaitDRQ(primary))
      return 1; /*If error,this media is not inserted.*/
   
   ideWaitInterrupt(&wait,device);
   
   ideInsl(primary,IDE_REG_DATA,2,ideIOBuffer); /*Read data.*/

   schedule(); /*Wait for another IRQ.*/
   ideWaitInterruptEnd(device);

   enablePreemption();
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
         return -1; /*It should never arrive here.*/
   u8 busMasterIDEStatus = ideInb(primary,IDE_REG_BMSTATUS);
   u8 status = ideInb(primary,IDE_REG_STATUS); /*Get status.*/
   if(!(busMasterIDEStatus & IDE_BMSTATUS_INTERRUPT))
      return -1; 
   ideOutb(primary,IDE_REG_BMSTATUS,IDE_BMSTATUS_INTERRUPT);
   ideOutb(primary,IDE_REG_BMCOMMAND,0x0);

   if(wait)
   {
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
   registerDriver(&ideDriver);
   return 0;
}

driverInitcall(initIDE);
