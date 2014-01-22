#include <core/const.h>
#include <filesystem/virtual.h>
#include <filesystem/block.h>
#include <lib/string.h>

static int iso9660Mount(BlockDevicePart *part,FileSystemMount *mount);
static int iso9660LookUp(VFSDentry *dentry,VFSDentry *result,const char *name);
static int iso9660Open(VFSDentry *dentry,VFSFile *file);
static int iso9660Read(VFSFile *file,void *buf,u64 size);
static int iso9660LSeek(VFSFile *file,u64 offset);

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
   .lseek = &iso9660LSeek
};

/*See also http://wiki.osdev.org/ISO_9660.*/
/*And http://en.wikipedia.org/wiki/ISO_9660.*/

static int iso9660Mount(BlockDevicePart *part,FileSystemMount *mount)
{
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
   mount->root->type = VFSDentryDir;
   mount->root->name = "/";
   mount->root->inode->start = *(u32 *)(buffer + 156 + 2);
   mount->root->inode->start *= 2048; 
       /*Location of extent (LBA) in both-endian format.*/
   mount->root->inode->size = (u64)-1; /*Unknow,set max.*/
   mount->root->inode->inodeStart = io.start + 156 + 2;
   mount->root->inode->operation = &iso9660INodeOperation;
   mount->root->inode->part = part;
   return 0;
}

static int iso9660LookUp(VFSDentry *dentry,VFSDentry *result,const char *name)
{
   if(dentry->type != VFSDentryDir)
      return -ENOTDIR;
   u8 buffer[2048];
   VFSINode *inode = dentry->inode;
   BlockIO io;
   u64 pos = 0;
   u8 isDir = 0,needRead = 1;
   u8 realLength = strlen(name);
   u8 length = realLength;
   for(int i = 0;i < length;++i)
      if(name[i] == '.')
        goto next;
   length++;
next:
   io.part = inode->part;
   io.read = 1;
   io.buffer = buffer;
   io.size = 2048;
   io.start = inode->start;
   for(;;pos += buffer[pos])/*Length of Directory Record.*/
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
      if(buffer[pos + 0] == 0)/*No more.*/
         return -ENOENT;
      isDir = buffer[pos + 25] & 0x2; /*Is it a dir?*/
      u8 nameLength = buffer[pos + 32];
      if(!isDir)
         nameLength -= 2; /*Remove ';1'.*/
      else
         nameLength += length - realLength;
      if(length != nameLength)
         continue;
      /*Length of file identifier (file name). 
       * This terminates with a ';' character 
       * followed by the file ID number in ASCII coded decimal ('1').*/

      char *filename = (char *)(buffer + pos + 33);
      if((!isDir) && (filename[0] == '_'))
         filename[0] = '.';
      if((!isDir) && (filename[length - 1] == '.'))
         filename[length - 1] = '\0';
      else
         filename[length] = '\0';

      /*To lower.*/
      for(u64 i = 0;i < length;++i)
         if((filename[i] <= 'Z') && (filename[i] >= 'A'))
            filename[i] -= 'A' - 'a';
      if(memcmp(name,filename,realLength) == 0)
         break;
   }
   result->type = isDir ? VFSDentryDir : VFSDentryFile;
   result->inode->inodeStart = io.start + pos;
   result->inode->size = *(u32 *)(buffer + pos + 10); /*Get size.*/
   result->inode->start = *(u32 *)(buffer + pos + 2);
   result->inode->start *= 2048;
      /*Location of extent (LBA) in both-endian format.*/
   result->inode->operation = &iso9660INodeOperation;
   result->inode->part = inode->part;
   return 0;
}

static int iso9660Open(VFSDentry *dentry,VFSFile *file)
{
   file->dentry = dentry;
   file->seek = 0;
   file->operation = &iso9660FileOperation;
   return 0;
}

static int iso9660Read(VFSFile *file,void *buf,u64 size)
{
   VFSINode *inode = file->dentry->inode;
   u64 seek = file->seek;
   BlockIO io;
   if(seek + size < seek) /*If too long,return.*/
      return -EINVAL;
   if(seek >= inode->size) /*If seek is not true,return.*/
      return -EBADFD;
   if(seek + size >= inode->size)
      size = inode->size - seek - 1; 
      /*Just set it too max bytes we can read.*/
   if(size == 0)
      return 0;
   io.part = inode->part;
   io.start = inode->start + seek;
   io.size = size;
   io.read = 1;
   io.buffer = buf;
   if(submitBlockIO(&io)) /*Try to read.*/
      return -EIO;
   file->seek += size; /*Add for reading next time.*/
   return size; /*Return how many bytes we have read.*/
}

static int iso9660LSeek(VFSFile *file,u64 offset)
{
   VFSINode *inode = file->dentry->inode;
   if(offset >= inode->size)
      return -EINVAL;
   file->seek = offset;
   return offset;
}

static int initISO9660(void)
{
   registerFileSystem(&iso9660FileSystem);
   return 0;
}

fileSystemInitcall(initISO9660);
