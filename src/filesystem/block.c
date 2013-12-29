#include <core/const.h>
#include <filesystem/block.h>
#include <filesystem/virtual.h>
#include <memory/kmalloc.h>

ListHead parts;
ListHead blockDevices;

static int initBlockDevice(void)
{
   return initList(&parts) | initList(&blockDevices);
}

static int syncBlockDevice(void)
{
   for(ListHead *list = parts.next;list != &parts;list = list->next)
   {
      BlockDevicePart *part = listEntry(list,BlockDevicePart,list);
      if(doMount("/",part) == 0)
         return 0; /*Try to mount.*/
   }
   return -1;
}

int registerBlockDevice(BlockDevice *device)
{
   switch(device->type)
   {
   case BlockDeviceCDROM:
      {
         BlockDevicePart *part
            = kmalloc(sizeof(BlockDevicePart));
         if(!part)
            return -1;
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
   default:
      return -1;
   }
   listAddTail(&device->list,&blockDevices);
   return 0;
}

int submitBlockIO(BlockIO *io)
{
   BlockDevicePart *part = io->part;
   BlockDevice *device = part->device;
   u64 size = io->size;
   u64 pos = io->start + part->start;
   if(pos + size < pos || pos + size > device->end)
      return -1;
   if(pos < io->start || pos < part->start)
      return -1;
   int ret = (*device->read)(device->data,pos,size,io->buffer);
   return ret; /*There should have an IO scheduler.*/
               /*And a waitForBlockIO function.But we don't.*/
}

subsysInitcall(initBlockDevice);
syncInitcall(syncBlockDevice);
