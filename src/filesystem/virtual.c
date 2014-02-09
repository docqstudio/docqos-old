#include <core/const.h>
#include <filesystem/block.h>
#include <filesystem/virtual.h>
#include <memory/kmalloc.h>
#include <task/task.h>
#include <lib/string.h>

static ListHead fileSystems; 
/*A list of file systems which has registered.*/

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
   atomicSet(&dentry->ref,1); /*Reference count.*/
   dentry->name = 0;
   dentry->mnt = 0;
   initSemaphore(&dentry->inode->semaphore);
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
   initList(&mnt->list);
   mnt->fs = 0;
   return mnt;
}

static int destoryFileSystemMount(FileSystemMount *mnt)
{
   u8 need = 1;
   if(mnt->fs) /*Check if 'mnt' is really mounted.*/
   {
      need = 0;
      lockSpinLock(&mnt->fs->lock);
      listDelete(&mnt->list);
      if(atomicAddRet(&mnt->root->ref,-1) == 0)
         need = 1; /*Need to destory mnt->root.*/
      unlockSpinLock(&mnt->fs->lock);
   }
   if(need)
      destoryDentry(mnt->root);
   return kfree(mnt);
}

static int vfsLookUpClear(VFSDentry *dentry)
{
   VFSDentry *parent;
   while(dentry)
   {
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

      VFSDentry *point;
      if(dentry->mnt)
         point = dentry->mnt->point;
      else
         point = dentry;

      if(point->parent)
         lockSpinLock(&point->parent->lock);
      atomicAdd(&point->ref,-1); /*It should never be 0.*/
      if(point->parent)
         unlockSpinLock(&point->parent->lock);
      dentry = point->parent;
   }
   return 0;
}

static VFSDentry *vfsLookUpDentry(VFSDentry *dentry)
{
   VFSDentry *child = dentry,*parent;
   while(child)
   {
      while((parent = child->parent))
      {
         lockSpinLock(&parent->lock);
         atomicAdd(&child->ref,1); /*Add the reference count.*/
         unlockSpinLock(&parent->lock);
         child = parent;
      }
      
      VFSDentry *point;
      if(dentry->mnt)
         point = child->mnt->point;
      else
         point = child;
      
      if(point->parent)
         lockSpinLock(&point->parent->lock);
      atomicAdd(&point->ref,1); /*Add the reference count,too!*/
      if(point->parent)
         unlockSpinLock(&point->parent->lock);
      child = point->parent;
   }
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
   if(!ret)
      return 0;
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
         VFSDentry *parent = ret->parent;
         if(!parent)
         { /*Now there are two logical possiblities:*/
           /*1. The 'dentry' is the root dentry.*/
           /*2. The 'dentry' is the root dentry for a FileSystemMount.*/
            parent = ret->mnt->point->parent;
            if(!parent)  /*The first possiblity,just goto next.*/
               goto next;
            FileSystemMount *mnt = ret->mnt;
            atomicAdd(&mnt->point->ref,-1); /*It should never be zero.*/
            ret = parent;
            goto next;
         }
         lockSpinLock(&parent->lock);
         if(atomicAddRet(&ret->ref,-1) == 0)
         { /*Destory the dentry if the reference count is zero.*/
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
            const VFSDentryType type = dentry->type;
            if(type == VFSDentryMount)
            { /*A mount point!*/
               FileSystemMount *mnt = dentry->mnt;
               dentry = mnt->root;
               unlockSpinLock(&ret->lock);
               ret = dentry;
               goto next;
            }
            if(type == VFSDentryDir)
               goto nextUnlock;
            unlockSpinLock(&ret->lock);
            ret = dentry;
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
         new->mnt = ret->mnt;
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
   if(path[0] == '.' && path[1] == '\0')
      return ret;
   if(path[0] == '.' && path[1] == '.' && path[2] == '\0')
   {
      VFSDentry *parent = ret->parent;
      if(!parent)
      {
         parent = ret->mnt->point->parent;
         if(!parent)
            return ret;
         lockSpinLock(&parent->lock);
         atomicAdd(&ret->mnt->point->ref,-1);
         unlockSpinLock(&parent->lock);
         return parent;
      }
      lockSpinLock(&parent->lock);
      if(atomicAdd(&ret->ref,-1) == 0)
      {
         listDelete(&ret->list);
         unlockSpinLock(&parent->lock);
         destoryDentry(ret);
         return parent;
      }
      unlockSpinLock(&parent->lock);
      return parent;
   }
   int pathLength = strlen(path);
   lockSpinLock(&ret->lock);
   for(ListHead *list = ret->children.next;list != &ret->children;list = list->next)
   {
      VFSDentry *dentry = listEntry(list,VFSDentry,list);
      if(memcmp(dentry->name,path,pathLength) == 0)
      { 
         atomicAdd(&dentry->ref,1);
         if(dentry->type == VFSDentryMount)
         { /*Get the really dir.*/
            FileSystemMount *mnt = dentry->mnt;
            dentry = mnt->root;
            unlockSpinLock(&ret->lock);
            ret = dentry;
            goto found;
         }
         unlockSpinLock(&ret->lock);
         ret = dentry;
         goto found; /*Found!! Just return.*/
      }
   }
   unlockSpinLock(&ret->lock);
   {
      new = createDentry(); /*Alloc a new dentry.*/
      if(unlikely(!new))
         goto failed;
      error = (*ret->inode->operation->lookUp)(ret,new,path);
      if(error)
         goto failedWithNew;
      new->parent = ret;
      new->mnt = ret->mnt;
      char *name = (char *)kmalloc(pathLength);
      if(unlikely(!name)) /*Fill the name field.*/
         goto failedWithNew;
      memcpy((void *)name,(const void *)path,pathLength);
      new->name = name;
      lockSpinLock(&ret->lock);
      listAddTail(&new->list,&ret->children); /*Add it.*/
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

static int vfsFillDir64(void *__data,u8 isDir,u64 length,const char *name)
{
   u8 *buf = ((u8 **)__data)[0];
   u64 size = ((u64 *)__data)[1];

   if(length + 3 >= size)
      return -EINVAL; /*The buffer is full!*/
   buf[0] = isDir;
   buf[1] = length;
   memcpy((void *)&buf[2],name,length); /*Copy to buffer.*/
   buf[2 + length] = '\0';
   size -= length + 3;
   buf += length + 3;
   ((u8 **)__data)[0] = buf;
   ((u64 *)__data)[1] = size;
   return 0;
}

static int initVFS(void)
{ /*Init this list.*/
   return initList(&fileSystems);
}


int registerFileSystem(FileSystem *system)
{
   initSpinLock(&system->lock);
   initList(&system->mounts);
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
      return (VFSFile *)makeErrorPointer(-ENOENT);
   VFSFile *file = kmalloc(sizeof(VFSFile));
   if(unlikely(!file))
   {
      vfsLookUpClear(dentry);
      return (VFSFile *)makeErrorPointer(-ENOMEM);
   }
   file->dentry = dentry;
   int ret = (*dentry->inode->operation->open)(dentry,file);
   if(ret)
   {
      kfree(file);
      vfsLookUpClear(dentry);
      return (VFSFile *)makeErrorPointer(ret);
   }
   return file;
}

int readFile(VFSFile *file,void *buf,u64 size)
{
   if(file->dentry->type == VFSDentryDir ||
      file->dentry->type == VFSDentryMount)
      return -EISDIR;
   if(!file->operation->read)
      return -EBADFD;
   int ret = (*file->operation->read)(file,buf,size);
   return ret; /*Call file->operation->read.*/
}

int writeFile(VFSFile *file,const void *buf,u64 size)
{
   if(file->dentry->type == VFSDentryDir)
      return -EISDIR;
   if(!file->operation->write)
      return -EBADFD;
   int ret = (*file->operation->write)(file,buf,size);
   return ret;
}

int lseekFile(VFSFile *file,u64 offset)
{
   if(file->dentry->type == VFSDentryDir)
      return -EISDIR;
   if(!file->operation->lseek)
      return -EBADFD;
   return (*file->operation->lseek)(file,offset);
}

int closeFile(VFSFile *file)
{
   VFSDentry *dentry = file->dentry;
   if(file->operation->close)
      (*file->operation->close)(file);
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
   return -EMFILE;
found:;
   VFSFile *file = openFile(path);
   if(isErrorPointer(file))
      return getPointerError(file);
   current->files->fd[fd] = file;
   return fd;
}

int doRead(int fd,void *buf,u64 size)
{
   if((unsigned int)fd >= TASK_MAX_FILES)
      return -EBADF;
   Task *current = getCurrentTask();
   VFSFile *file = current->files->fd[fd];
   if(!file)
      return -EBADF;
   return readFile(file,buf,size); /*Call file->operation->read.*/
}

int doWrite(int fd,const void *buf,u64 size)
{
   if((unsigned int)fd >= TASK_MAX_FILES)
      return -EBADF;
   Task *current = getCurrentTask();
   VFSFile *file = current->files->fd[fd];
   if(!file)
      return -EBADF;
   return writeFile(file,buf,size); /*Call file->operation->write.*/
}

int doLSeek(int fd,u64 offset)
{
   if((unsigned int)fd >= TASK_MAX_FILES)
      return -EBADF;
   Task *current = getCurrentTask();
   VFSFile *file = current->files->fd[fd];
   if(!file)
      return -EBADF;
   return lseekFile(file,offset);
}

int doClose(int fd)
{
   if((unsigned int)fd >= TASK_MAX_FILES)
      return -EBADF;
   Task *current = getCurrentTask();
   VFSFile *file = current->files->fd[fd];
   if(!file) /*If there are no files,return.*/
      return -EBADF;
   current->files->fd[fd] = 0;
   return closeFile(file);
}

int doGetDents64(int fd,void *data,u64 size)
{
   if((unsigned int)fd >= TASK_MAX_FILES)
      return -EBADF;
   u64 __data[] = {(u64)data,size};
   Task *current = getCurrentTask();
   VFSFile *file = current->files->fd[fd];
   if(!file)
      return -EBADF;
   if(file->dentry->type != VFSDentryDir)
      return -ENOTDIR;
   int ret = (*file->operation->readDir)(file,&vfsFillDir64,__data);
   if(ret < 0) /*Call the readdir function.*/
      return 0;
   return ((void **)__data)[0] - data;
}

int doChroot(const char *dir)
{
   VFSDentry *dentry = vfsLookUp(dir);
   if(!dentry)
      return -ENOENT;
   if(dentry->type != VFSDentryDir)
      return (vfsLookUpClear(dentry),-ENOTDIR);
   Task *current = getCurrentTask();
   vfsLookUpClear(current->fs->root);
   current->fs->root = dentry;
   if(!current->fs->pwd)
      current->fs->pwd = dentry;
   return 0;
}

int doUMount(const char *point)
{
   if(point[0] == '/' && point[1] == '\0')
      return -EPERM; /*Can not umount / .*/
   int ret;
   VFSDentry *dentry = vfsLookUp(point);
   FileSystemMount *mnt; /*Look for this dentry*/
   ret = -EINVAL;
   if(dentry->parent)
      goto out;
   dentry = dentry->mnt->point;
   ret = -EBUSY;
   lockSpinLock(&dentry->parent->lock);
   if(atomicRead(&dentry->ref) > 2)
      goto unlockOut; /*If this is used,failed.*/
   dentry->type = VFSDentryDir; /*Restore to dir type.*/
   mnt = dentry->mnt; /*Get mnt and set to the parent's mnt.*/
   dentry->mnt = dentry->parent->mnt;
   unlockSpinLock(&dentry->parent->lock);
   destoryFileSystemMount(mnt); /*Destory it.*/
   vfsLookUpClear(dentry);
   vfsLookUpClear(dentry);
   return 0;
unlockOut:
   unlockSpinLock(&dentry->lock);
out:
   vfsLookUpClear(dentry);
   return ret;
}

int doMount(const char *point,FileSystem *fs,
                  BlockDevicePart *part,u8 init)
{
   FileSystemMount *mnt = createFileSystemMount();
   VFSDentry *old = mnt->root;
   if(!mnt)
      return -ENOMEM;
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
   return -EINVAL;
found:
   mnt->fs = fs;
   if(old != mnt->root)
      destoryDentry(old);
   if(part)
      part->fileSystem = fs;
   if(mnt->root->type != VFSDentryDir)
      goto failed;
   VFSDentry *dentry = vfsLookUp(point);
   if(!dentry) /*Find the dentry of the mount point.*/
      goto failed;
   mnt->point = dentry;
   mnt->root->parent = 0; 
   mnt->root->mnt = mnt;
   if(!dentry->parent && dentry->mnt)
   {
      vfsLookUpClear(dentry);
      destoryFileSystemMount(mnt);
      return -EBUSY;
   }

   if(dentry->type != VFSDentryDir)
   {
      vfsLookUpClear(dentry);
      destoryFileSystemMount(mnt);
      return -ENOTDIR;
   }
   if(likely(dentry->parent))
      lockSpinLock(&dentry->parent->lock);
   int reference = atomicRead(&dentry->ref);
   if(!init && reference > 1)
   {
      if(likely(dentry->parent))
         unlockSpinLock(&dentry->parent->lock);
      vfsLookUpClear(dentry);
      destoryFileSystemMount(mnt);
      return -EBUSY;
   }
   dentry->mnt = mnt;
   dentry->type = VFSDentryMount; /*Change type.*/
   if(likely(dentry->parent))
      unlockSpinLock(&dentry->parent->lock);
   return 0;
}

int mountRoot(BlockDevicePart *part)
{  /*Only use in kernel init.*/
   Task *current = getCurrentTask();
   if(!current->fs->root)
   {  /*Create a root dentry.*/
      VFSDentry *root = createDentry();
      if(unlikely(!root))
         return -ENOMEM;
      kfree(root->inode); /*We don't need the inode.*/
      root->parent = 0;
      root->inode = 0;
      root->type = VFSDentryDir;
      current->fs->root = current->fs->pwd = root;
   }

   int ret = doMount("/",0,part,1);
   if(ret)
      return ret;
   current->fs->root = current->fs->root->mnt->root;
   current->fs->pwd = current->fs->root;
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
   new->root = new->pwd = 0;
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

VFSFile *cloneFile(VFSFile *file)
{
   VFSFile *new = kmalloc(sizeof(*new));
   if(unlikely(!new))
      return 0;
   new->dentry = vfsLookUpDentry(file->dentry);
   new->operation = file->operation;
   new->seek = file->seek;
   return new;
}

subsysInitcall(initVFS);
