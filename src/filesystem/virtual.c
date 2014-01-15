#include <core/const.h>
#include <filesystem/block.h>
#include <filesystem/virtual.h>
#include <memory/kmalloc.h>
#include <task/task.h>
#include <lib/string.h>

ListHead fileSystems; 
/*A list of file systems which has registered.*/

static int initVFS(void)
{ /*Init this list.*/
   return initList(&fileSystems);
}

static VFSDentry *createDentry(void)
{
   VFSDentry *dentry = (VFSDentry *)kmalloc(sizeof(VFSDentry));
   if(unlikely(!dentry)) /*No memory.*/
      return 0;
   dentry->inode = (VFSINode *)kmalloc(sizeof(VFSINode));
   if(unlikely(!dentry->inode))
   {
      kfree(dentry);
      return 0;
   } /*Init some fields.*/
   initSpinLock(&dentry->lock);
   initList(&dentry->list);
   initList(&dentry->children);
   atomicSet(&dentry->ref,1); /*Ref count.*/
   dentry->name = 0;
   dentry->mnt = 0;
   return dentry;
}

static int destoryDentry(VFSDentry *dentry)
{
   if(dentry->name)
      kfree(dentry->name);
   kfree(dentry->inode);
   return kfree(dentry); /*Kfree them.*/
}

static FileSystemMount *createFileSystemMount(void)
{
   FileSystemMount *mnt = 
      (FileSystemMount *)kmalloc(sizeof(FileSystemMount));
   if(unlikely(!mnt)) /*No memory.*/
      return 0;
   mnt->root = createDentry();
   if(unlikely(!mnt->root))
   {
      kfree(mnt);
      return 0;
   }
   return mnt;
}

static int destoryFileSystemMount(FileSystemMount *mnt)
{
   destoryDentry(mnt->root);
   return kfree(mnt);
}

static int vfsLookUpClear(VFSDentry *dentry)
{
   VFSDentry *parent;
   while((parent = dentry->parent))
   {
      lockSpinLock(&parent->lock);
      if(atomicAddRet(&dentry->ref,-1) == 0)
      { /*If reference count is zero,destory it.*/
         listDelete(&dentry->list);
         unlockSpinLock(&parent->lock);
         destoryDentry(dentry);
         dentry = parent;
         continue;
      }
      unlockSpinLock(&parent->lock);
      dentry = parent;
   }
   atomicAdd(&dentry->ref,-1); /*It should never be 0.*/
   return 0;
}

static VFSDentry *vfsLookUpDentry(VFSDentry *dentry)
{
   VFSDentry *child = dentry,*parent;
   while((parent = child->parent))
   {
      lockSpinLock(&parent->lock);
      atomicAdd(&child->ref,1);
      unlockSpinLock(&parent->lock);
      child = parent;
   }
   atomicAdd(&child->ref,1);
   return dentry;
}

static VFSDentry *vfsLookUp(const char *__path)
{
   if(*__path == '\0')
      return 0;
   VFSDentry *ret,*new;
   u8 length = strlen(__path);
   char ___path[length + 1];
   char *path = ___path; 
   u8 error = 0; /*Copy path for writing.*/
   memcpy((void *)path,(const void *)__path,length + 1);
   if(*path == '/')
      ret = getCurrentTask()->fs->root;
   else
      ret = getCurrentTask()->fs->pwd;
   ret = vfsLookUpDentry(ret);
   for(;;)
   {
      while(*path == '/')
         ++path;
      if(*path == '\0')
         return ret;
      char *next = path;
      while(*next != '/' && *next != '\0')
         ++next;
      if(*next == '\0')
         break; /*If last,break.*/
      *next = '\0'; /*It will be restored soon.*/
      int pathLength = strlen(path);
      if(path[0] == '.' && path[1] == '\0')
         goto next;
      if(path[0] == '.' && path[1] == '.' && path[2] == '\0')
      {
         if(!ret->parent)
            goto next;
         VFSDentry *parent = ret->parent;
         lockSpinLock(&parent->lock);
         if(atomicAddRet(&ret->ref,-1) == 0)
         {
            listDelete(&ret->list);
            unlockSpinLock(&parent->lock);
            destoryDentry(ret);
            ret = parent;
            goto next;
         }
         ret = parent;
         goto next;
      }
      lockSpinLock(&ret->lock);
      for(ListHead *list = ret->children.next;list != &ret->children;list = list->next)
      {
         VFSDentry *dentry = listEntry(list,VFSDentry,list);
         if(memcmp(dentry->name,path,pathLength) == 0)
         { /*Search ret->children.*/
            atomicAdd(&dentry->ref,1);
            unlockSpinLock(&ret->lock);
            ret = dentry;
            lockSpinLock(&ret->lock);
            const VFSDentryType type = ret->type;
            if(type == VFSDentryDir)
               goto nextUnlock;
            if(type == VFSDentryMount)
            {
               FileSystemMount *mnt = dentry->mnt;
               dentry = mnt->root;
               atomicAdd(&dentry->ref,1);
               unlockSpinLock(&ret->lock);
               ret = dentry;
               goto next;
            }
            unlockSpinLock(&ret->lock);
            goto failed;
         }
      }
      unlockSpinLock(&ret->lock);
      {
         new = createDentry();
         if(unlikely(!new))
            goto failed;
         error = (*ret->inode->operation->lookUp)(ret,new,path);
         if(error || new->type != VFSDentryDir) /*Try to look it up in disk.*/
            goto failedWithNew;
         new->parent = ret;
         char *name = (char *)kmalloc(pathLength + 1);
         if(unlikely(!name)) /*If no memory for name,exit.*/
            goto failedWithNew;
         memcpy((void *)name,(const void *)path,pathLength + 1);
         new->name = name;
         lockSpinLock(&ret->lock);
         listAddTail(&new->list,&ret->children);
         unlockSpinLock(&ret->lock); /*Add to parent's children list.*/
         ret = new;
         goto next;
      }
nextUnlock:
      unlockSpinLock(&ret->lock);
next:
      *next = '/'; /*Restore.*/
      path = next;
   }
    /*Last.*/
   int pathLength = strlen(path);
   lockSpinLock(&ret->lock);
   for(ListHead *list = ret->children.next;list != &ret->children;list = list->next)
   {
      VFSDentry *dentry = listEntry(list,VFSDentry,list);
      if(memcmp(dentry->name,path,pathLength) == 0)
      { 
         atomicAdd(&ret->ref,1);
         unlockSpinLock(&ret->lock);
         ret = dentry;
         goto found; /*Found!! Just return.*/
      }
   }
   unlockSpinLock(&ret->lock);
   {
      new = createDentry();
      if(unlikely(!new))
         goto failed;
      error = (*ret->inode->operation->lookUp)(ret,new,path);
      if(error)
         goto failedWithNew;
      new->parent = ret;
      char *name = (char *)kmalloc(pathLength);
      if(unlikely(!name))
         goto failedWithNew;
      memcpy((void *)name,(const void *)path,pathLength);
      new->name = name;
      lockSpinLock(&ret->lock);
      listAddTail(&new->list,&ret->children);
      unlockSpinLock(&ret->lock);
      ret = new;
   }
found:
   return ret;
failedWithNew:
   destoryDentry(new);
failed: /*Failed.*/
   vfsLookUpClear(ret);
   return 0;
}

int registerFileSystem(FileSystem *system)
{
   return listAddTail(&system->list,&fileSystems);
} /*In fact,maybe there need a spinlock,but now we just ingore it.*/

FileSystem *lookForFileSystem(const char *name)
{
   int len = strlen(name) + 1;
   for(ListHead *list = fileSystems.next;list != &fileSystems;list = list->next)
   {
      FileSystem *sys = listEntry(list,FileSystem,list);
      if(memcmp(sys->name,name,len) == 0)
         return sys;
   }
   return 0;
}

BlockDevicePart *openBlockDeviceFile(const char *path)
{
   VFSDentry *dentry = vfsLookUp(path);
   if(!dentry)
      return 0;
   if(dentry->type != VFSDentryBlockDevice)
      goto failed; 
      /*Don't need lock dentry->lock.*/
      /*Block device file's type will never change.*/
      /*Instead,a VFSDentryDir dentry can change to a VFSDentryMount dentry.*/
      /*A VFSDentryMount dentry can also change to a VFSDentryDir dentry.*/
      /*So if we check 'if(dentry->type == VFSDentryMount)',we need to lock dentry->lock.*/
   BlockDevicePart *part = dentry->inode->part;
   vfsLookUpClear(dentry);
   return part;
failed:
   vfsLookUpClear(dentry);
   return 0;
}

VFSFile *openFile(const char *path)
{
   VFSDentry *dentry = vfsLookUp(path);
   if(!dentry)
      return 0;
   if(dentry->type == VFSDentryDir)
   {
      vfsLookUpClear(dentry);
      return 0; /*Return -1 if failed.*/
   }
   VFSFile *file = kmalloc(sizeof(VFSFile));
   if(unlikely(!file))
   {
      vfsLookUpClear(dentry);
      return 0;
   }
   file->dentry = dentry;
   int ret = (*dentry->inode->operation->open)(dentry,file);
   if(ret)
   {
      kfree(file);
      vfsLookUpClear(dentry);
      return 0;
   }
   return file;
}

int readFile(VFSFile *file,void *buf,u64 size)
{
   if(!file->operation->read)
      return -1;
   int ret = (*file->operation->read)(file,buf,size);
   return ret; /*Call file->operation->read.*/
}

int writeFile(VFSFile *file,const void *buf,u64 size)
{
   if(!file->operation->write)
      return -1;
   int ret = (*file->operation->write)(file,buf,size);
   return ret;
}

int lseekFile(VFSFile *file,u64 offset)
{
   if(!file->operation->lseek)
      return -1;
   return (*file->operation->lseek)(file,offset);
}

int closeFile(VFSFile *file)
{
   VFSDentry *dentry = file->dentry;
   vfsLookUpClear(dentry); /*Clear it.*/
   kfree(file);
   return 0;
}

int doOpen(const char *path)
{
   Task *current = getCurrentTask();
   int fd;
   for(fd = 0;
      fd < sizeof(current->files->fd) / sizeof(current->files->fd[0]);++fd)
      if(current->files->fd[fd] == 0)
         goto found; /*Found a null position in current->fd.*/
   return -1;
found:
   current->files->fd[fd] = openFile(path); /*Done!*/
   return current->files->fd[fd] ? fd : -1;
}

int doRead(int fd,void *buf,u64 size)
{
   if((unsigned int)fd >= TASK_MAX_FILES)
      return -1;
   Task *current = getCurrentTask();
   VFSFile *file = current->files->fd[fd];
   if(!file)
      return -1;
   return readFile(file,buf,size); /*Call file->operation->read.*/
}

int doWrite(int fd,const void *buf,u64 size)
{
   if((unsigned int)fd >= TASK_MAX_FILES)
      return -1;
   Task *current = getCurrentTask();
   VFSFile *file = current->files->fd[fd];
   if(!file)
      return -1;
   return writeFile(file,buf,size); /*Call file->operation->write.*/
}

int doLSeek(int fd,u64 offset)
{
   if((unsigned int)fd >= TASK_MAX_FILES)
      return 0;
   Task *current = getCurrentTask();
   VFSFile *file = current->files->fd[fd];
   if(!file)
      return -1;
   return lseekFile(file,offset);
}

int doClose(int fd)
{
   if((unsigned int)fd >= TASK_MAX_FILES)
      return -1;
   Task *current = getCurrentTask();
   VFSFile *file = current->files->fd[fd];
   if(!file) /*If there are no files,return -1.*/
      return -1;
   current->files->fd[fd] = 0;
   return closeFile(file);
}

int doChroot(FileSystemMount *mnt)
{ /*It's too bad ,but now we just do this.*/
   Task *current = getCurrentTask();
   current->fs->root = mnt->root;
   if(!current->fs->pwd)
      current->fs->pwd = mnt->root;
   return 0;
}

int doUMount(const char *point)
{
   if(point[0] == '/' && point[1] == '\0')
      return -1; /*Can not umount / .*/
   int ret;
   VFSDentry *dentry = vfsLookUp(point);
   FileSystemMount *mnt; /*Look for this dentry*/
   ret = -1;
   if(dentry->type != VFSDentryMount)
      goto out; 
   lockSpinLock(&dentry->lock);
   if(atomicRead(&dentry->mnt->root->ref) > 1)
      goto unlockOut; /*If this is used,failed.*/
   dentry->type = VFSDentryDir; /*Restore to dir type.*/
   mnt = dentry->mnt; /*Get mnt and set to 0.*/
   dentry->mnt = 0;
   unlockSpinLock(&dentry->lock);
   destoryFileSystemMount(mnt); /*Destory it.*/
   return 0;
unlockOut:
   unlockSpinLock(&dentry->lock);
out:
   vfsLookUpClear(dentry);
   return ret;
}

int doMount(const char *point,FileSystem *fs,BlockDevicePart *part)
{
   FileSystemMount *mnt = createFileSystemMount();
   if(!mnt)
      return -1;
   if(fs && (*fs->mount)(part,mnt) == 0)
      goto found;
   if(!part)
      goto failed;
   fs = part->fileSystem;
   if(fs && (*fs->mount)(part,mnt) == 0)
      goto found;

   for(ListHead *list = fileSystems.next;list != &fileSystems;list = list->next)
   {
      fs = listEntry(list,FileSystem,list); 
         /*Look for the file system.*/
      if((*fs->mount)(part,mnt) == 0)
         goto found;
   }
failed:
   destoryFileSystemMount(mnt);
   return -1;
found:
   if(part)
      part->fileSystem = fs;
   if(mnt->root->type != VFSDentryDir)
      return (destoryFileSystemMount(mnt),-1);
   if(point[0] == '/' && point[1] == '\0')
   {
      mnt->parent = 0;
      mnt->root->parent = 0; /*No parents.*/
      doChroot(mnt); /*Call chroot.*/
      return 0;
   }
   VFSDentry *dentry = vfsLookUp(point);
   if(!dentry) /*Find the dentry of the mount point.*/
      goto failed;
   if(dentry->type != VFSDentryDir)
   {
      vfsLookUpClear(dentry);
      goto failed;
   }
   mnt->parent = 0; /*Unused,just do this.*/
   mnt->root->parent = dentry; 
   lockSpinLock(&dentry->lock);
   dentry->mnt = mnt;
   dentry->type = VFSDentryMount; /*Change type.*/
   unlockSpinLock(&dentry->lock);
   return 0;
}

TaskFileSystem *taskForkFileSystem(TaskFileSystem *old,ForkFlags flags)
{
   if(flags & ForkShareFileSystem)
   {
      if(!old)
         return old;
      atomicAdd(&old->ref,1);
      return old;
   }
   TaskFileSystem *new = kmalloc(sizeof(*new));
   atomicSet(&new->ref,1);
   if(old && old->root)
      new->root = vfsLookUpDentry(old->root); /*Add old->root's reference count.*/
   if(old && old->pwd)
      new->pwd = vfsLookUpDentry(old->pwd); /*Add old->pwd's reference count.*/
   return new;
}

int taskExitFileSystem(TaskFileSystem *old)
{
   if(atomicAddRet(&old->ref,-1) == 0)
   {      /*If old's reference count is zero,free it.*/
      if(old->root)
         vfsLookUpClear(old->root);
      if(old->pwd)
         vfsLookUpClear(old->pwd);
      kfree(old);
   }
   return 0;
}

TaskFiles *taskForkFiles(TaskFiles *old,ForkFlags flags)
{
   if(flags & ForkShareFiles)
   {
      if(!old)
         return old;
      atomicAdd(&old->ref,1);
      return old;
   }
   TaskFiles *new = kmalloc(sizeof(*new));
   if(unlikely(!new))
      return new;
   memset(new->fd,0,sizeof(new->fd));
   atomicSet(&new->ref,1);
   if(!old)
      goto out;
   for(int i = 0;i < sizeof(old->fd)/sizeof(old->fd[0]);++i)
   {
      if(old->fd[i])
      { /*Copy file descriptors.*/
         VFSFile *file = kmalloc(sizeof(*file));
         if(unlikely(!file))
            continue;
         file->seek = old->fd[i]->seek;
         file->operation = old->fd[i]->operation;
         file->dentry = vfsLookUpDentry(old->fd[i]->dentry);
                               /*Add old->fd[i]->dentry's reference count.*/
         new->fd[i] = file;
      }
   }
out:
   return new;
}

int taskExitFiles(TaskFiles *old,u8 share)
{
   if(atomicAddRet(&old->ref,-1) == 0)
   {
      for(int i = 0;i < sizeof(old->fd) / sizeof(old->fd[0]);++i)
         if(old->fd[i])
            closeFile(old->fd[i]); /*Close files which are opened.*/
      kfree(old);
   }
   return 0;
}

subsysInitcall(initVFS);
