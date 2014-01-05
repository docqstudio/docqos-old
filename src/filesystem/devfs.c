#include <core/const.h>
#include <core/list.h>
#include <filesystem/virtual.h>
#include <filesystem/block.h>
#include <memory/kmalloc.h>
#include <lib/string.h>

typedef struct DevfsINode{
   SpinLock lock;
   char name[15];
   BlockDevicePart *part;
   VFSFileOperation *operation;
   ListHead children;
   ListHead list;
} DevfsINode;

int devfsRegisterBlockDevice(BlockDevicePart *part,const char *name);
static int devfsMount(BlockDevicePart *part __attribute__ ((unused))
   ,FileSystemMount *mnt);
static int devfsLookUp(VFSDentry *dentry,VFSDentry *result,const char *name);
static int devfsOpen(VFSDentry *dentry,VFSFile *file);

static FileSystem devfs = {
   .name = "devfs",
   .mount = devfsMount
};

static VFSINodeOperation devfsINodeOperation = {
   .lookUp = devfsLookUp,
   .mkdir = 0,
   .unlink = 0,
   .open = devfsOpen
};

static DevfsINode devfsRoot;

static int devfsLookUp(VFSDentry *dentry,VFSDentry *result,const char *name)
{
   int len = strlen(name) + 1; /*Add 1 for '\0'.*/
   DevfsINode *inode = (DevfsINode *)dentry->inode->data; /*Get data.*/
   for(ListHead *list = inode->children.next;list != &inode->children;list = list->next)
   {
      DevfsINode *next = listEntry(list,DevfsINode,list);
          /*Foreach its child dirs.*/
      if(memcmp(next->name,name,len) == 0) /*Is is the dir we are looking for?*/
      {
         result->type = VFSDentryBlockDevice; /*Block device file.*/
         result->inode->part = next->part; 
         result->inode->operation = dentry->inode->operation;
         result->inode->data = next;
         return 0;
      }
   }
   return -1;
}

static int devfsMount(BlockDevicePart *part __attribute__ ((unused))
   ,FileSystemMount *mnt)
{ /*Just ingore part.*/
   mnt->root->inode->data = &devfsRoot; /*Set some fields.*/
   mnt->root->type = VFSDentryDir;
   mnt->root->inode->operation = &devfsINodeOperation;
   return 0;
}

static int devfsOpen(VFSDentry *dentry,VFSFile *file)
{
   DevfsINode *inode = (DevfsINode *)dentry->inode->data;
   if(!inode->operation)
      return -1;
   file->operation = inode->operation;
   file->dentry = dentry;
   file->seek = 0;
   return 0;
}

static int devfsInit(void)
{
   initList(&devfsRoot.children);
   devfsRoot.name[0] = '/';
   devfsRoot.name[1] = '\0';
   initSpinLock(&devfsRoot.lock);
      /*Set some fields of devfsRoot.*/
   return 0;
}

static int devfsRegister(void)
{
   registerFileSystem(&devfs); /*Register devfs.*/
   return 0;
}

int devfsRegisterBlockDevice(BlockDevicePart *part,const char *name)
{
   while(*name != '\0' && *name == '/')
      ++name; /*Skip '/'.*/
   if(*name == '\0')
      return -1; /*If name only has '/',return.*/
   if(unlikely(!part))
      return -1; /*If part is 0,return.*/
   int len = strlen(name); /*Get length of name.*/
   if(len > 14)
      return -1;
   for(int i = 0;i < len;++i)
      if(name[i] == '/')
         return -1; /*If name has '/',return.*/
   DevfsINode *inode = kmalloc(sizeof(*inode));
   if(unlikely(!inode)) /*No memory,return.*/
      return -1;
   memcpy(inode->name,name,len + 1);
   inode->part = part;
   inode->operation = 0;
   lockSpinLock(&devfsRoot.lock);
   listAdd(&inode->list,&devfsRoot.children);
   unlockSpinLock(&devfsRoot.lock); 
      /*Add this block device file to devfs' root dir.*/
   return 0;
}

int devfsRegisterDevice(VFSFileOperation *operation,const char *name)
{
   while(*name != '\0' && *name == '/')
      ++name; /*Skip '/'.*/
   if(*name == '\0')
      return -1; /*If name only has '/',return.*/
   if(unlikely(!operation))
      return -1; /*If operation is 0,return.*/
   int len = strlen(name); /*Get length of name.*/
   if(len > 14)
      return -1;
   for(int i = 0;i < len;++i)
      if(name[i] == '/')
         return -1; /*If name has '/',return.*/
   DevfsINode *inode = kmalloc(sizeof(*inode));
   if(unlikely(!inode)) /*No memory,return.*/
      return -1;
   memcpy(inode->name,name,len + 1);
   inode->part = 0;
   inode->operation = operation;
   lockSpinLock(&devfsRoot.lock);
   listAdd(&inode->list,&devfsRoot.children);
   unlockSpinLock(&devfsRoot.lock); 
      /*Add this block device file to devfs' root dir.*/
   return 0;
}

subsysInitcall(devfsInit);
fileSystemInitcall(devfsRegister);
