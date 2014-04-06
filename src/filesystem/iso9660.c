#include <core/const.h>
#include <core/math.h>
#include <filesystem/virtual.h>
#include <block/block.h>
#include <lib/string.h>
#include <video/console.h>
#include <memory/buddy.h>

static int iso9660Mount(BlockDevicePart *part,FileSystemMount *mount);
static int iso9660LookUp(VFSDentry *dentry,VFSDentry *result,const char *name);
static int iso9660Open(VFSDentry *dentry,VFSFile *file,int mode);
static int iso9660Read(VFSFile *file,void *buf,u64 size,u64 *seek);
static int iso9660LSeek(VFSFile *file,s64 offset,int type);
static int iso9660ReadDir(VFSFile *file,VFSDirFiller filler,void *data);
static int iso9660PutPage(PhysicsPage *page);
static PhysicsPage *iso9660GetPage(VFSINode *inode,u64 offset);

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

static PageCacheOperation iso9660PageCacheOperation = {
   .getPage = &iso9660GetPage,
   .putPage = &iso9660PutPage,
   .flushPage = 0
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
   mount->root->inode->size = *(u32 *)(buffer + 156 + 10);
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

static int iso9660ReadPage(VFSINode *inode,PhysicsPage *page,unsigned int index)
{
   BlockIO io = {
      .part = inode->part,
      .start = (index << 12) + inode->start,
      .size = 4096,
      .buffer = getPhysicsPageAddress(page),
      .read = 1
   };
   return submitBlockIO(&io);
}

static PhysicsPage *iso9660GetPage(VFSINode *inode,u64 offset)
{
   downSemaphore(&inode->semaphore);
   PhysicsPage *retval =
         getPageFromPageCache(&inode->cache,offset >> 12,&iso9660ReadPage);
   upSemaphore(&inode->semaphore);
   return retval;
}

static int iso9660PutPage(PhysicsPage *page)
{
   downSemaphore(&page->cache->inode->semaphore);
   putPageIntoPageCache(page);
   upSemaphore(&page->cache->inode->semaphore);
   return 0;
}

static int iso9660LookUp(VFSDentry *dentry,VFSDentry *result,const char *name)
{
   if(!S_ISDIR(dentry->inode->mode))
      return -ENOTDIR;
   VFSINode *inode = dentry->inode;
   u8 length = strlen(name);
   u64 pos = 0,realPosition = 0;
   PhysicsPage *page = getPageFromPageCache(&inode->cache,0,&iso9660ReadPage);
   if(!page) /*Get the page from the page cache.*/
      return -EIO;
   u8 *buffer = getPhysicsPageAddress(page);
       /*Get the page address.*/

   const char *filename;
   u64 mode,size,lba;
   u8 namelength;

   while(realPosition < inode->size)
   {
      u8 retval;
      if(pos >= 0x1000)
      {
         pos &= 0xfff;
         putPageIntoPageCache(page); /*Put this page.*/
         page = getPageFromPageCache(&inode->cache,realPosition >> 12,&iso9660ReadPage);
         if(!page) /*Get the page again.*/
            return -EIO;
         buffer = getPhysicsPageAddress(page);
      }
      retval = iso9660GetDentryInformation(
         &buffer[pos],&filename,&mode,&size,&lba,&namelength);
               /*Get the data.*/
      if(!retval)
      {
         pos = (pos + 0x7ff) & ~0x7ff; /*Next block.*/
         realPosition = (realPosition + 0x7ff) & ~0x7ff;
         continue;
      }
      pos += retval;
      realPosition += retval;
      if(length != namelength)
         continue;
      if(memcmp(name,filename,length))
         continue;
      result->inode->inodeStart = inode->start + pos;
      result->inode->size = size;
      result->inode->start = lba * 2048;
      result->inode->operation = &iso9660INodeOperation;
      result->inode->part = inode->part;
      result->inode->mode = mode;
      result->inode->cache.operation = &iso9660PageCacheOperation;
            /*Init the fields of result.*/
      putPageIntoPageCache(page);
      return 0;
   }
   putPageIntoPageCache(page);
   return -ENOENT;
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
   u64 retval = 0;
   VFSINode *inode = file->dentry->inode;
   if(*seek + size < *seek) /*If too long,return.*/
      return -EINVAL;
   if(*seek >= inode->size) /*If seek is not true,return.*/
      return 0;
   if(*seek + size >= inode->size)
      size = inode->size - *seek - 1; 
      /*Just set it too max bytes we can read.*/
   if(size == 0)
      return 0;

   downSemaphore(&inode->semaphore);
   while(size > 0)
   {
      PhysicsPage *page = getPageFromPageCache(
              &inode->cache,*seek >> 12,&iso9660ReadPage);
                  /*Get the page.*/
      if(!page)
         return -EIO;
      void *addr = getPhysicsPageAddress(page);
      u64 read = min(size,0x1000 - (*seek & 0xfff));
      memcpy(buf,addr + (*seek & 0xfff),read); /*Copy to the buffer.*/
      *seek += read;
      size -= read;
      retval += read;
      putPageIntoPageCache(page); /*Put the page.*/
   }
   upSemaphore(&inode->semaphore);

   return retval; /*Return how many bytes we have read.*/
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
   VFSINode *inode = file->dentry->inode;
   PhysicsPage *page;
   u8 *buf;
   u64 pos,realPosition;

   downSemaphore(&inode->semaphore);
   pos = realPosition = file->seek;
   page = 
      getPageFromPageCache(&inode->cache,pos >> 12,&iso9660ReadPage);
   if(!page)
      goto failed;
   buf = getPhysicsPageAddress(page);
   pos &= 0xfff;
   while(realPosition < inode->size)
   {
      if(pos >= 0x1000) 
      {
         pos &= 0xfff;
         putPageIntoPageCache(page);
         page = getPageFromPageCache(&inode->cache,realPosition >> 12,&iso9660ReadPage);
         if(!page) /*Get the page again.*/
            return -EIO;
         buf = getPhysicsPageAddress(page);
      }
      const char *filename;
      u64 mode,size,lba;
      u8 length;
      u8 retval = iso9660GetDentryInformation(
         &buf[pos],&filename,&mode,&size,&lba,&length);
               /*Get the data.*/
      if(!retval)
      {
         realPosition = (realPosition + 0x7ff) & ~0x7ff;
         pos = (pos + 0x7ff) & ~0x7ff;
         continue;
      }
      pos += retval;
      realPosition += retval;
      if((*filler)(data,!!S_ISDIR(mode),length,filename) < 0)
         break; /*Fill the buffer.*/
   }
   u64 old = file->seek;
   file->seek = realPosition; /*Update the seek.*/
   putPageIntoPageCache(page);
   upSemaphore(&inode->semaphore);
   return realPosition - old;
failed:
   putPageIntoPageCache(page); /*Put the page.*/
   upSemaphore(&inode->semaphore);
   return -EIO;
}

static int initISO9660(void)
{
   registerFileSystem(&iso9660FileSystem);
   return 0;
}

fileSystemInitcall(initISO9660);
