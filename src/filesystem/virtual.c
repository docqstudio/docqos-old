#include <core/const.h>
#include <core/hlist.h>
#include <cpu/rcu.h>
#include <filesystem/block.h>
#include <filesystem/virtual.h>
#include <memory/kmalloc.h>
#include <task/task.h>
#include <lib/string.h>

#define VFS_DENTRY_CACHE_COUNT 1024
#define VFS_DENTRY_CACHE_MASK  1023

static ListHead fileSystems; 
/*A list of file systems which has registered.*/
static HashListHead vfsDentryCache[VFS_DENTRY_CACHE_COUNT];
                    /*Dentry Cache Hash Table.*/
static RCULock vfsDentryCacheRCU; /*For reading.*/
static SpinLock vfsDentryCacheLock; /*For writing.*/

static u64 vfsHashName(const char *name,u64 *phash)
{
   u64 retval = 0,hash = 0;
   unsigned long c;
   while((c = *name++) != '\0')
   {
      hash = (hash + (c << 4) + (c >> 4)) * 11;
      ++retval; 
   }
   *phash = hash;
   return retval; /*Return the length of name.*/
}

static u64 __vfsHashDentry(VFSDentry *parent,u64 hash)
{
   hash += (u64)parent / (1 << 7);
   hash += hash >> 32;
   hash &= VFS_DENTRY_CACHE_MASK; /*Mask the hash number.*/
   return hash;
}

static int vfsHashDentry(VFSDentry *dentry)
{
   u64 hash = __vfsHashDentry(dentry->parent,dentry->hash);
   lockSpinLock(&vfsDentryCacheLock);
   hashListHeadAdd(&dentry->node,&vfsDentryCache[hash]); /*Add to the dentry cache.*/
   unlockSpinLock(&vfsDentryCacheLock);
   return 0;
}

static int vfsUnhashDentry(VFSDentry *dentry)
{
   lockSpinLock(&vfsDentryCacheLock);
   hashListDelete(&dentry->node);  /*Delete from the dentry cache.*/
   unlockSpinLock(&vfsDentryCacheLock);
   return 0;
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
   initHashListNode(&dentry->node);
   atomicSet(&dentry->ref,1); /*Reference count.*/
   dentry->name = 0;
   dentry->mnt = 0;
   dentry->mounted = 0;
   initSemaphore(&dentry->inode->semaphore);
   return dentry;
}

static int __destoryDentry(void *data)
{
   VFSDentry *dentry = data;
   if(dentry->name)
      kfree(dentry->name);
   kfree(dentry->inode);
   return kfree(dentry); /*Free them.*/
}

static int destoryDentry(VFSDentry *dentry)
{
   int old;
   do {
      old = atomicRead(&dentry->ref);
      if(old != 0)  /*Can not destory the dentries which are used.*/
         return -EBUSY;
   } while(atomicCompareExchange(&dentry->ref,old,-1) != old);

   if(!hashListEmpty(&dentry->node))
      vfsUnhashDentry(dentry); /*Unhash.*/
   else
      return __destoryDentry(dentry);
   return addRCUCallback(&vfsDentryCacheRCU,&__destoryDentry,dentry);
}

static VFSFile *createFile(VFSDentry *dentry)
{
   VFSFile *retval = kmalloc(sizeof(*retval));
   if(unlikely(!retval))
      return 0;
   atomicSet(&retval->ref,1);
   retval->seek = 0;
   retval->dentry = dentry;
   return retval;
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
      __destoryDentry(mnt->root);
   return kfree(mnt);
}

static VFSDentry *vfsDentryCacheLookUp(VFSDentry *parent,u64 hash,const char *s,u64 length)
{
   int old;
   VFSDentry *retval = 0;
   FileSystemMount *mnt = 0;
   u64 dhash = __vfsHashDentry(parent,hash);
   lockRCUReadLock(&vfsDentryCacheRCU);
   for(HashListNode *node = vfsDentryCache[dhash].first;node;node = node->next)
   {    /*Look for the dentry cache.*/
      VFSDentry *dentry = hashListEntry(node,VFSDentry,node);
      if(dentry->hash == hash && memcmp(dentry->name,s,length) == 0)
      {
         do {
            old = atomicRead(&dentry->ref);
            if(old < 0) /*The dentry is destorying,discard it!*/
               break;
         } while(atomicCompareExchange(&dentry->ref,old,old + 1) != old);
         if(old < 0)
            continue; /*Destorying.*/
         if(old & (1 << 16)) /*Mounted?*/
            while((mnt = *(FileSystemMount *volatile *)&dentry->mounted) == 0)
               ; /*Wait for the dentry->mounted set..*/
         if(mnt)
            dentry = mnt->root;
         retval = dentry;
         break;
      }
   }
   unlockRCUReadLock(&vfsDentryCacheRCU);
   return retval;
}

static int vfsLookUpClear(VFSDentry *dentry)
{
   VFSDentry *parent;
   while(dentry)
   {
      while((parent = dentry->parent))
      {
         if(atomicAddRet(&dentry->ref,-1) == 0)
         { /*If reference count is zero,destory it.*/
            destoryDentry(dentry);
            dentry = parent;
            continue;
         }
         dentry = parent;
      }

      VFSDentry *point;
      if(dentry->mnt)
         point = dentry->mnt->point;
      else
         point = dentry;

      atomicAdd(&point->ref,-1); /*It should never be 0.*/
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
         atomicAdd(&child->ref,1); /*Add the reference count.*/
         child = parent;
      }
      
      VFSDentry *point;
      if(dentry->mnt)
         point = child->mnt->point;
      else
         point = child;
      
      atomicAdd(&point->ref,1); /*Add the reference count,too!*/
      child = point->parent;
   }
   return dentry;
}

static VFSDentry *vfsLookUp(const char *__path)
{
   if(*__path == '\0')
      return 0;
   Task *current = getCurrentTask();
   VFSDentry *ret,*new;
   u8 length = strlen(__path);
   char ___path[length + 1];
   char *path = ___path; 
   u64 hash;
   u8 error = 0; /*Copy path for writing.*/
   memcpy((void *)path,(const void *)__path,length + 1);
   if(*path == '/')
      ret = current->fs->root;
   else
      ret = current->fs->pwd;
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
      int pathLength = vfsHashName(path,&hash) + 1;
      if(path[0] == '.' && path[1] == '\0')
         goto next;
      if(path[0] == '.' && path[1] == '.' && path[2] == '\0')
      {
         VFSDentry *parent = ret->parent;
         if(!parent)
         { /*Now there are two logical possiblities:*/
           /*1. The 'dentry' is the root dentry.*/
           /*2. The 'dentry' is the root dentry for a FileSystemMount.*/
            if(!ret->mnt->point->parent || ret == current->fs->root)  
               goto next; /*The first possiblity,just goto next.*/
            FileSystemMount *mnt = ret->mnt;
            atomicAdd(&mnt->point->ref,-1); /*It should never be zero.*/
            ret = mnt->point->parent;
            goto next;
         }
         if(atomicAddRet(&ret->ref,-1) == 0)
         { /*Destory the dentry if the reference count is zero.*/
            destoryDentry(ret);
            ret = parent;
            goto next;
         }
         ret = parent;
         goto next;
      }
      VFSDentry *dentry = vfsDentryCacheLookUp(ret,hash,path,pathLength);
      if(dentry)
      {
         const VFSDentryType type = dentry->type;
         if(type == VFSDentryDir &&
            (ret = dentry))
            goto next;
         ret = dentry;
         goto failed;
      }
      downSemaphore(&ret->inode->semaphore);
      dentry = vfsDentryCacheLookUp(ret,hash,path,pathLength);
                /*Look for the dentry cache again.*/
      if(likely(!dentry))
      {
         new = createDentry();
         if(unlikely(!new) && (upSemaphore(&ret->inode->semaphore) || 1))
            goto failed;
         error = (*ret->inode->operation->lookUp)(ret,new,path);
         if((error || new->type != VFSDentryDir) && (upSemaphore(&ret->inode->semaphore) || 1)) 
                                                    /*Try to look it up in disk.*/
            goto failedWithNew;
         new->parent = ret;
         new->mnt = ret->mnt;
         char *name = (char *)kmalloc(pathLength + 1);
         if(unlikely(!name) && (upSemaphore(&ret->inode->semaphore) || 1)) /*If no memory for name,exit.*/
            goto failedWithNew;
         memcpy((void *)name,(const void *)path,pathLength + 1);
         new->name = name;
         new->hash = hash; /*The hash number.*/
         vfsHashDentry(new);
         dentry = new;
      }
      upSemaphore(&ret->inode->semaphore);
      ret = dentry;
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
         if(!ret->mnt->point->parent || ret == current->fs->root) /*The root dentry.*/
            return ret;
         atomicAdd(&ret->mnt->point->ref,-1);
         return ret->mnt->point->parent;
      }
      if(atomicAddRet(&ret->ref,-1) == 0)
      {
         destoryDentry(ret);
         return parent;
      }
      return parent;
   }
   int pathLength = vfsHashName(path,&hash) + 1;
   VFSDentry *dentry = vfsDentryCacheLookUp(ret,hash,path,pathLength);
   if(dentry && (ret = dentry))
      goto found; /*Found!! Just return.*/
   downSemaphore(&ret->inode->semaphore);
   dentry = vfsDentryCacheLookUp(ret,hash,path,pathLength);
                        /*Look for the dentry cache again.*/
   if(likely(!dentry))
   {
      new = createDentry(); /*Alloc a new dentry.*/
      if(unlikely(!new) && (upSemaphore(&ret->inode->semaphore) || 1))
         goto failed;
      error = (*ret->inode->operation->lookUp)(ret,new,path);
      if(error && (upSemaphore(&ret->inode->semaphore) || 1))
         goto failedWithNew;
      new->parent = ret;
      new->mnt = ret->mnt;
      char *name = (char *)kmalloc(pathLength);
      if(unlikely(!name) && (upSemaphore(&ret->inode->semaphore) || 1)) 
                                 /*Fill the name field.*/
         goto failedWithNew;
      memcpy((void *)name,(const void *)path,pathLength);
      new->name = name;
      new->hash = hash;
      vfsHashDentry(new);
      dentry = new;
   }
   upSemaphore(&ret->inode->semaphore);
   ret = dentry;
found:
   return ret;
failedWithNew:
   __destoryDentry(new);
failed: /*Failed.*/
   vfsLookUpClear(ret);
   return 0;
}

static int destoryFile(VFSFile *file)
{
   vfsLookUpClear(file->dentry);
   kfree(file);
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
   initSpinLock(&vfsDentryCacheLock);
   initRCULock(&vfsDentryCacheRCU);
   for(int i = VFS_DENTRY_CACHE_COUNT - 1;i >= 0;--i)
      initHashListHead(&vfsDentryCache[i]);
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
   VFSFile *file = createFile(dentry);
   if(unlikely(!file))
   {
      vfsLookUpClear(dentry);
      return (VFSFile *)makeErrorPointer(-ENOMEM);
   }
   int ret = (*dentry->inode->operation->open)(dentry,file);
   if(ret)
   {
      kfree(file);
      vfsLookUpClear(dentry);
      return (VFSFile *)makeErrorPointer(ret);
   }
   return file;
}

int closeFile(VFSFile *file)
{
   if(atomicAddRet(&file->ref,-1) != 0)
      return 0; /*It is used,just return.*/
   if(file->operation->close)
      (*file->operation->close)(file);
   return destoryFile(file);
}

int readFile(VFSFile *file,void *buf,u64 size)
{
   if(file->dentry->type == VFSDentryDir)
      return -EISDIR;
   if(!file->operation->read)
      return -EBADFD;
   int ret = (*file->operation->read)(file,buf,size,&file->seek);
   return ret; /*Call file->operation->read.*/
}

int writeFile(VFSFile *file,const void *buf,u64 size)
{
   if(file->dentry->type == VFSDentryDir)
      return -EISDIR;
   if(!file->operation->write)
      return -EBADFD;
   int ret = (*file->operation->write)(file,buf,size,&file->seek);
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

int doChdir(const char *dir)
{
   VFSDentry *dentry = vfsLookUp(dir);
   if(!dentry)
      return -ENOENT;
   if(dentry->type != VFSDentryDir)
      return (vfsLookUpClear(dentry),-ENOTDIR);
   Task *current = getCurrentTask();
   if(current->fs->pwd)
      vfsLookUpClear(current->fs->pwd);
   current->fs->pwd = dentry;
   return 0;
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
   doChdir("/");
   return 0;
}

int doUMount(const char *point)
{
   if(point[0] == '/' && point[1] == '\0')
      return -EPERM; /*Can not umount / .*/
   int ret,old;
   VFSDentry *dentry = vfsLookUp(point);
   FileSystemMount *mnt; /*Look for this dentry*/
   ret = -EINVAL;
   if(dentry->parent)
      goto out;
   dentry = dentry->mnt->point;
   ret = -EBUSY;
   do {
      old = atomicRead(&dentry->ref);
      if(old != ((1 << 16) + 2))
         goto out; /*If this is used,failed.*/
   }while(atomicCompareExchange(&dentry->ref,old,old - (1 << 16)) != old);
   mnt = dentry->mounted; /*Get mnt and set to 0.*/
   dentry->mounted = 0;
   destoryFileSystemMount(mnt); /*Destory it.*/
   vfsLookUpClear(dentry);
   vfsLookUpClear(dentry);
   return 0;
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
      __destoryDentry(old);
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
   int ref;
   disablePreemption();
   if(init && (atomicSet(&dentry->ref,(1 << 16) + 2) || 1))
      goto out;
   do {
      ref = atomicRead(&dentry->ref);
      if(ref != 1)
      {
         vfsLookUpClear(dentry);
         destoryFileSystemMount(mnt);
         return -EBUSY;
      }
   } while(atomicCompareExchange(&dentry->ref,ref,ref + (1 << 16)) != ref);
out:
   dentry->mounted = mnt;
   enablePreemption();
   return 0;
}

int doGetCwd(char *buf,u64 size)
{
   Task *current = getCurrentTask();
   VFSDentry *pwd = current->fs->pwd;
   VFSDentry *root = current->fs->root;
   const char *names[10];
   int j = 1;
   u8 length = 0;
   if(size < 2)
      return -EINVAL;

   while(pwd && pwd != root) 
   {
      if(!pwd->parent)
         pwd = pwd->mnt->point;
      if(!pwd)
         break;

      if(length >= sizeof(names) / sizeof(names[0]))
         return -EOVERFLOW;
      names[length++] = pwd->name;
               /*Save the file names to 'names'.*/
      pwd = pwd->parent;
   }

   buf[0] = '/'; /*Start with the '/' char.*/
   buf[1] = '\0';
   if((size -= 2) == 0 && length > 0)
      return -EOVERFLOW;
   if(!length)
      return 1;
   
   for(int i = length - 1;i >= 0;--i)
   {
      const char *name = names[i];
      u8 slen = strlen(name);
      if(j + slen + 1 > size)
         return -EOVERFLOW; /*Add 1 for '\0' or '/'.*/
      memcpy((void *)&buf[j],(const void *)name,slen);
                 /*Copy the name to the buffer.*/
      buf[j + slen] = ((i == 0) ? '\0' : '/');
      j += slen + 1;
   }
   return j - 1;
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
      current->fs->root = root;
      current->fs->pwd = 0;
   }

   int ret = doMount("/",0,part,1);
   if(ret)
      return ret;
   VFSDentry *root = current->fs->root->mounted->root;
   current->fs->root = root;
   current->fs->pwd = vfsLookUpDentry(root);
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
      new->fd[i] = vfsGetFile(old->fd[i]);
      /*Copy the files.*/
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

VFSFile *vfsGetFile(VFSFile *file)
{
   if(!file)
      return 0;
   atomicAdd(&file->ref,1);
      /*Add the reference count of the file.*/
   return file;
}

subsysInitcall(initVFS);
