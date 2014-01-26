#include <core/const.h>
#include <core/list.h>
#include <filesystem/virtual.h>
#include <filesystem/block.h>
#include <memory/kmalloc.h>
#include <lib/string.h>

typedef struct DevfsINode{
   char name[15];
   BlockDevicePart *part;
   VFSFileOperation *operation;
   ListHead children;
   ListHead list;
} DevfsINode;

static VFSDentry *devfsRootDentry = 0;

static int devfsMount(BlockDevicePart *part __attribute__ ((unused))
   ,FileSystemMount *mnt);
static int devfsLookUp(VFSDentry *dentry,VFSDentry *result,const char *name);
static int devfsOpen(VFSDentry *dentry,VFSFile *file);
static int devfsReadDir(VFSFile *file,VFSDirFiller filler,void *data);
static int devfsClose(VFSFile *file);

static FileSystem devfs = {
   .name = "devfs",
   .mount = &devfsMount
};

static VFSFileOperation devfsRootOperation = {
   .read = 0,
   .write = 0,
   .lseek = 0,
   .readDir = devfsReadDir,
   .close = devfsClose
};

static VFSINodeOperation devfsINodeOperation = {
   .lookUp = &devfsLookUp,
   .mkdir = 0,
   .unlink = 0,
   .open = &devfsOpen
};

static DevfsINode devfsRoot;

static int devfsReadDir(VFSFile *file,VFSDirFiller filler,void *data)
{
   DevfsINode *inode;
   ListHead *p;
   
   inode = file->data;
   if(!inode) /*Now we have alreay read the last dentry,so return.*/
      return 0;
   downSemaphore(&file->dentry->inode->semaphore);
   switch(file->seek)
   {
   case 0:
      if((*filler)(data,1,1,".") < 0)
         break;
      ++file->seek;
   case 1:
      if((*filler)(data,1,1,"..") < 0)
         break;
      ++file->seek;
   default:
      for(p = &inode->list;p != &devfsRoot.children;p = p->next)
      {
         DevfsINode *d = listEntry(p->next,DevfsINode,list);
         if(d->name[0] == '\0')
            continue;
         if((*filler)(data,0,strlen(d->name),d->name) < 0)
            break;
      }
      listDelete(&inode->list);
      if(p == &devfsRoot.list) /*Last?*/
         (file->data = 0),(kfree(&inode));
      else
         listAddTail(&inode->list,p); /*Add again!*/
      break;
   }
   upSemaphore(&file->dentry->inode->semaphore);
   return 0;
}

static int devfsClose(VFSFile *file)
{
   if(file->data)
   {
      DevfsINode *inode = file->data;
      downSemaphore(&file->dentry->inode->semaphore);
      listDelete(&inode->list);
      upSemaphore(&file->dentry->inode->semaphore);
      kfree(inode); /*Free it!*/
   }
   return 0;
}

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
         result->inode->operation = &devfsINodeOperation;
         result->inode->data = next;
         return 0;
      }
   }
   return -ENOENT;
}

static int devfsMount(BlockDevicePart *part __attribute__ ((unused))
   ,FileSystemMount *mnt)
{ /*Just ingore part.*/
   FileSystem *fs = &devfs;
   mnt->root->inode->data = &devfsRoot; /*Set some fields.*/
   mnt->root->type = VFSDentryDir;
   mnt->root->inode->operation = &devfsINodeOperation;
   
   lockSpinLock(&fs->lock);
   if(listEmpty(&fs->mounts))
      goto out;
   FileSystemMount *mount = listEntry(fs->mounts.next,FileSystemMount,list);
   atomicAdd(&mount->root->ref,1);
   mnt->root = mount->root;
out:
   unlockSpinLock(&fs->lock);

   devfsRootDentry = mnt->root;
   return 0;
}

static int devfsOpen(VFSDentry *dentry,VFSFile *file)
{
   DevfsINode *inode = (DevfsINode *)dentry->inode->data;
   if(!inode->operation)
      return -EINVAL;
   file->operation = inode->operation;
   file->dentry = dentry;
   file->seek = 0;
   if(dentry == devfsRootDentry)
   {
      DevfsINode *inode = kmalloc(sizeof(*inode));
      if(!inode)
         return -ENOMEM;
      inode->name[0] = 0;
      file->data = inode;
      downSemaphore(&dentry->inode->semaphore);
      listAdd(&inode->list,&devfsRoot.children); /*Add it.*/
      upSemaphore(&dentry->inode->semaphore);
   }
   return 0;
}

static int devfsInit(void)
{
   initList(&devfsRoot.children);
   devfsRoot.name[0] = '/';
   devfsRoot.name[1] = '\0';
   devfsRoot.operation = &devfsRootOperation;
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
      return -EINVAL; /*If name only has '/',return.*/
   if(unlikely(!part))
      return -EINVAL; /*If part is 0,return.*/
   int len = strlen(name); /*Get length of name.*/
   if(len > 14)
      return -ENAMETOOLONG;
   for(int i = 0;i < len;++i)
      if(name[i] == '/')
         return -EINVAL; /*If name has '/',return.*/
   DevfsINode *inode = kmalloc(sizeof(*inode));
   if(unlikely(!inode)) /*No memory,return.*/
      return -ENOMEM;
   memcpy(inode->name,name,len + 1);
   inode->part = part;
   inode->operation = 0;

   if(devfsRootDentry)
      downSemaphore(&devfsRootDentry->inode->semaphore);
   listAdd(&inode->list,&devfsRoot.children);
   if(devfsRootDentry)
      upSemaphore(&devfsRootDentry->inode->semaphore);
      /*Add this block device file to devfs' root dir.*/
   return 0;
}

int devfsRegisterDevice(VFSFileOperation *operation,const char *name)
{
   while(*name != '\0' && *name == '/')
      ++name; /*Skip '/'.*/
   if(*name == '\0')
      return -EINVAL; /*If name only has '/',return.*/
   if(unlikely(!operation))
      return -EINVAL; /*If operation is 0,return.*/
   int len = strlen(name); /*Get length of name.*/
   if(len > 14)
      return -ENAMETOOLONG;
   for(int i = 0;i < len;++i)
      if(name[i] == '/')
         return -EINVAL; /*If name has '/',return.*/
   DevfsINode *inode = kmalloc(sizeof(*inode));
   if(unlikely(!inode)) /*No memory,return.*/
      return -ENOMEM;
   memcpy(inode->name,name,len + 1);
   inode->part = 0;
   inode->operation = operation;

   if(devfsRootDentry)
      downSemaphore(&devfsRootDentry->inode->semaphore);
   listAdd(&inode->list,&devfsRoot.children);
      /*Add this block device file to devfs' root dir.*/
   if(devfsRootDentry)
      upSemaphore(&devfsRootDentry->inode->semaphore);
   return 0;
}

subsysInitcall(devfsInit);
fileSystemInitcall(devfsRegister);
