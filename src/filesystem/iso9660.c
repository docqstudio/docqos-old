#include <core/const.h>
#include <filesystem/virtual.h>
#include <filesystem/block.h>
#include <lib/string.h>
#include <video/console.h>

static int iso9660Mount(BlockDevicePart *part,FileSystemMount *mount);
static int iso9660LookUp(VFSDentry *dentry,VFSDentry *result,const char *name);
static int iso9660Open(VFSDentry *dentry,VFSFile *file,int mode);
static int iso9660Read(VFSFile *file,void *buf,u64 size,u64 *seek);
static int iso9660LSeek(VFSFile *file,s64 offset,int type);
static int iso9660ReadDir(VFSFile *file,VFSDirFiller filler,void *data);

static FileSystem iso9660FileSystem = {
   .mount = &iso9660Mount,
   .name = "iso9660"
};

static VFSINodeOperation iso9660INodeOperation = {
   .mkdir = 0,
   .unlink = 0,
   .lookUp = &iso9660LookUp,
   .open = &iso9660Open
};

static VFSFileOperation iso9660FileOperation = {
   .read = &iso9660Read,
   .write = 0,
   .lseek = &iso9660LSeek,
   .readDir = &iso9660ReadDir,
   .close = 0
};

/*See also http://wiki.osdev.org/ISO_9660.*/
/*And http://en.wikipedia.org/wiki/ISO_9660.*/

static int iso9660Mount(BlockDevicePart *part,FileSystemMount *mount)
{
   FileSystem *const fs = &iso9660FileSystem;
   u8 buffer[2048];
   BlockIO io;
   io.part = part;
   io.size = sizeof(buffer);
   io.buffer = buffer;
   io.read = 1;
   io.start = 0x8000;

   /*System Area (32,768 B)     Unused by ISO 9660*/
   /*32786 = 0x8000.*/
   for(;;io.start += 2048)
   {
      if(submitBlockIO(&io) < 0)
         return -EIO;
      /*Identifier is always "CD001".*/
      if(buffer[1] != 'C' ||
         buffer[2] != 'D' ||
         buffer[3] != '0' ||
         buffer[4] != '0' ||
         buffer[5] != '1' )
         return -EPROTO;
      if(buffer[0] == 0xff) /*Volume Descriptor Set Terminator.*/
         return -EPROTO;
      if(buffer[0] != 0x01) /*Primary Volume Descriptor.*/
         continue;
      break;
   }      
   /*Directory entry for the root directory.*/
   if(buffer[156] != 0x22)
      return -EPROTO;
   mount->root->inode->mode = S_IFDIR | S_IRWXU; 
   mount->root->name = 0;
   mount->root->inode->start = *(u32 *)(buffer + 156 + 2);
   mount->root->inode->start *= 2048; 
       /*Location of extent (LBA) in both-endian format.*/
   mount->root->inode->size = (u64)-1; /*Unknow,set max.*/
   mount->root->inode->inodeStart = io.start + 156 + 2;
   mount->root->inode->operation = &iso9660INodeOperation;
   mount->root->inode->part = part;

   FileSystemMount *mnt = 0;
   lockSpinLock(&fs->lock);
   for(ListHead *list = fs->mounts.next;list != &fs->mounts;
             list = list->next)
   {
      mnt = listEntry(list,FileSystemMount,list);
      if(mnt->root->inode->part == part)
         goto found;
   }
   unlockSpinLock(&fs->lock);
   return 0;
found:
   atomicAdd(&mnt->root->ref,1);
   unlockSpinLock(&fs->lock);
   mount->root = mnt->root;
   return 0;
}

static u8 iso9660GetDentryInformation(void *pdata,const char **pfilename,
                   u64 *ppermission,u64 *psize,u64 *plba,u8 *pnamelength)
{
   u8 *data = pdata;
   s64 size = data[0];
   u8 dir = data[25] & 0x2; 
   u8 length = data[32]; /*Filename Length.*/
   char *const filename = (void *)&data[33]; /*ISO9660 Filename.*/
   const char *rfilename = 0; /*Rock Brigde Filename.*/
   u64 mode = 0;
   
   if(!size)
      return 0;

   if(length + 33 >= size) /*No more data,not rock ridge filesystem.*/
      goto iso;
   data += length + 33;
   size -= length - 33;
   if(*data == '\0')
      --size,++data; /*Why do we need do this?*/
   while(size > 0)
   {
      switch(*(u16 *)&data[0])
      {
      case ((u16)'P' + ((u16)'X' << 8)): /*POSIX file attributes.*/
         if(data[2] < 12 || data[3] < 1) /*Too little data.*/
            break;
         mode = *(u32 *)&data[4]; 
         break;
      case ((u16)'N' + ((u16)'M' << 8)): /*Alternate name.*/
         if(data[2] < 6 || data[3] < 1)
            break;
         if(data[4] & 1) /*Not support,just continue.*/
            break;
         if(data[4] & 2) /*Current.*/
            (rfilename = "."),(length = 1);
         else if(data[4] & 4) /*Parent.*/
            (rfilename = ".."),(length = 2);
         else
            (rfilename = (void *)&data[5]),(length = data[2] - 5);
               /*Get the filename.*/
         break;
      default:
         break;
      }
      if(data[2] == 0)
         break; /*No more data.*/
      size -= data[2];
      data += data[2];
   }

   if(ppermission && mode)
      *ppermission = mode;
   if(pfilename && rfilename)
      *pfilename = rfilename;
   if(pnamelength)
      *pnamelength = length;
   if(rfilename)
      goto nofilename; /*Don't get the iso9660 filename.*/
                      /*We want to use the rock ridge filename.*/

iso:
   if(!dir)
      length -= 2;
   if(length == 1 && filename[0] == 0)
   { /*The dot directory.*/
      if(pfilename)
         *pfilename = ".";
      if(pnamelength)
         *pnamelength = 1;
      goto nofilename;
   }
   if(length == 1 && filename[0] == 1)
   { /*The dotdot directory.*/
      if(pfilename)
         *pfilename = "..";
      if(pnamelength)
         *pnamelength = 2;
      goto nofilename;
   }
   if(filename[0] == '_')
      filename[0] = '.'; /*Set to '.'.*/
   if(filename[length - 1] == '.')
      length -= 1;
   for(int i = 0;i < length;++i)
      if(*filename >= 'A' && *filename <= 'Z')
         *filename -= 'A' - 'a';
   if(pfilename)
      *pfilename = filename;
   if(pnamelength)
      *pnamelength = length;

nofilename:
   if(ppermission && dir && !mode)
      *ppermission = S_IFDIR | S_IRWXU;
   else if(ppermission && !mode)
      *ppermission = S_IFREG | S_IRWXU;
        /*If we didn't get the mode,set them to the default value.*/
   data = pdata;
   if(psize)
      *psize = *(u32 *)&data[10];
   if(plba)
      *plba = *(u32 *)&data[2];
   return *data;
}

static int iso9660LookUp(VFSDentry *dentry,VFSDentry *result,const char *name)
{
   if(!S_ISDIR(dentry->inode->mode))
      return -ENOTDIR;
   u8 buffer[2048];
   VFSINode *inode = dentry->inode;
   BlockIO io;
   u8 needRead = 1,length = strlen(name);
   u64 pos = 0;

   const char *filename;
   u64 mode,size,lba;
   u8 namelength;

   io.part = inode->part;
   io.read = 1;
   io.buffer = buffer;
   io.size = 2048;
   io.start = inode->start;
   for(;;)
   {
      while(pos > 2048)
      {
         pos -= 2048;
         io.start += 0x800;
         needRead = 1; /*Read again.*/
      }
      if(needRead)
         if(submitBlockIO(&io))
            return -EIO;
      needRead = 0;
      u8 retval = iso9660GetDentryInformation(
         &buffer[pos],&filename,&mode,&size,&lba,&namelength);
               /*Get the data.*/
      if(!retval)
         return -ENOENT;
      pos += retval;
      if(length != namelength)
         continue;
      if(memcmp(name,filename,length))
         continue;
      result->inode->inodeStart = io.start + pos;
      result->inode->size = size;
      result->inode->start = lba * 2048;
      result->inode->operation = &iso9660INodeOperation;
      result->inode->part = inode->part;
      result->inode->mode = mode;
      return 0;
   }
   return -EINVAL; /*Never arrive here.*/
}

static int iso9660Open(VFSDentry *dentry,VFSFile *file,int mode)
{
   if((mode & O_ACCMODE) != O_RDONLY)
      return -EINVAL;
   file->dentry = dentry;
   
   file->seek = 0;
   file->operation = &iso9660FileOperation;
   return 0;
}

static int iso9660Read(VFSFile *file,void *buf,u64 size,u64 *seek)
{
   VFSINode *inode = file->dentry->inode;
   BlockIO io;
   if(*seek + size < *seek) /*If too long,return.*/
      return -EINVAL;
   if(*seek >= inode->size) /*If seek is not true,return.*/
      return 0;
   if(*seek + size >= inode->size)
      size = inode->size - *seek - 1; 
      /*Just set it too max bytes we can read.*/
   if(size == 0)
      return 0;
   io.part = inode->part;
   io.start = inode->start + *seek;
   io.size = size;
   io.read = 1;
   io.buffer = buf;
   if(submitBlockIO(&io)) /*Try to read.*/
      return -EIO;
   *seek += size; /*Add for reading next time.*/
   return size; /*Return how many bytes we have read.*/
}

static int iso9660LSeek(VFSFile *file,s64 offset,int type)
{
   VFSINode *inode = file->dentry->inode;
   switch(type)
   {
   case SEEK_SET:
      file->seek = offset;
      break;
   case SEEK_CUR:
      file->seek += offset;
      break;
   case SEEK_END:
      file->seek = inode->size - 1 + offset;
      break;
   default:
      return -EINVAL;
   }
   return file->seek;
}

static int iso9660ReadDir(VFSFile *file,VFSDirFiller filler,void *data)
{
   u8 buf[2048];
   BlockIO io;
   u64 pos = 0;
   u8 needRead = 1;

   io.read = 1;
   io.part = file->dentry->inode->part;
   io.start = file->seek + file->dentry->inode->start;
   io.size = sizeof(buf);
   io.buffer = buf;
   for(;;)
   {
      while(pos > 2048)
         (pos -= 2048),(io.start += 2048),(needRead = 1);
      if(needRead)
         if(submitBlockIO(&io) < 0)
            return -EIO; /*I/O Error!*/
      const char *filename;
      u64 mode,size,lba;
      u8 length;
      u8 retval = iso9660GetDentryInformation(
         &buf[pos],&filename,&mode,&size,&lba,&length);
               /*Get the data.*/
      if(!retval)
         break;
      pos += retval;
      if((*filler)(data,!!S_ISDIR(mode),length,filename) < 0)
         break; /*Fill the buffer.*/
   }
   u64 old = file->seek;
   file->seek = pos; /*Update the seek.*/
   return pos - old;
}

static int initISO9660(void)
{
   registerFileSystem(&iso9660FileSystem);
   return 0;
}

fileSystemInitcall(initISO9660);
