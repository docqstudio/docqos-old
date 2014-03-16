#include <core/const.h>
#include <block/block.h>
#include <filesystem/virtual.h>
#include <filesystem/devfs.h>
#include <memory/kmalloc.h>
#include <lib/string.h>

static ListHead parts;
static ListHead blockDevices;

static int initBlockDevice(void)
{
   return initList(&parts) | initList(&blockDevices);
}

static int syncBlockDevice(void)
{
   for(ListHead *list = parts.next;list != &parts;list = list->next)
   {
      BlockDevicePart *part = listEntry(list,BlockDevicePart,list);
      if(!mountRoot(part))
         return 0; /*Try to mount.*/
   }
   return -ENODEV;
}

int registerBlockDevice(BlockDevice *device,const char *devfs)
{
   switch(device->type)
   {
   case BlockDeviceCDROM:
      {
         BlockDevicePart *part
            = kmalloc(sizeof(BlockDevicePart));
         if(!part)
            return -ENOMEM;
         part->next = 0;
         part->start = 0;
         part->end = device->end;
         part->fileSystem = 0;
         part->device = device;
         device->parts = part;
         device->partCount = 1;
         listAddTail(&part->list,&parts);
         /*CDROM has only one part.*/
      }
      break;
   case BlockDeviceDisk: /*No support.*/
      return -ENOSYS;
   default:
      return -EINVAL;
   }
   listAddTail(&device->list,&blockDevices);
   if(devfs)
   {
      BlockDevicePart *part = device->parts;
      int len = strlen(devfs);
      char name[len + 2];
      memcpy(name,devfs,len);
      name[len + 0] = '0';
      name[len + 1] = '\0';
      do{ /*Register the parts of this block device to devfs.*/
         devfsRegisterBlockDevice(part,name);
         ++name[len + 0]; /*Like these: sda0,sda1,sda2.....*/
      }while((part = part->next));
   }
   return 0;
}

int submitBlockIO(BlockIO *io)
{
   BlockDevicePart *part = io->part;
   BlockDevice *device = part->device;
   u64 size = io->size;
   u64 pos = io->start + part->start;
   if(pos + size < pos || pos + size > device->end)
      return -EINVAL;  
   if(pos < io->start || pos < part->start)
      return -EINVAL;
   int ret = (*device->read)(device->data,pos,size,io->buffer);
   return ret; /*There should have an IO scheduler.*/
               /*And a waitForBlockIO function.But we don't.*/
}

subsysInitcall(initBlockDevice);
syncInitcall(syncBlockDevice);
