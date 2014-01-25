#pragma once
#include <core/const.h>
#include <core/list.h>
#include <cpu/spinlock.h>
#include <cpu/atomic.h>

typedef struct BlockDevicePart BlockDevicePart;
typedef struct VFSINode VFSINode;
typedef struct VFSFile VFSFile;
typedef struct VFSDentry VFSDentry;
typedef struct FileSystemMount FileSystemMount;

typedef enum VFSDentryType{
   VFSDentryFile,
   VFSDentryDir,
   VFSDentryMount,
   VFSDentryBlockDevice
} VFSDentryType;

typedef struct TaskFileSystem{
   AtomicType ref;
   VFSDentry *root;
   VFSDentry *pwd;
} TaskFileSystem;

typedef struct TaskFiles{
   AtomicType ref;
   VFSFile *fd[TASK_MAX_FILES];
} TaskFiles;

typedef struct VFSFileOperation
{
   int (*read)(VFSFile *file,void *buf,u64 size);
   int (*write)(VFSFile *file,const void *buf,u64 size);
   int (*lseek)(VFSFile *file,u64 offset);
} VFSFileOperation;

typedef struct VFSINodeOperation
{
   int (*mkdir)(VFSDentry *dentry,const char *name);
   int (*unlink)(VFSDentry *result);
   int (*lookUp)(VFSDentry *dentry,VFSDentry *result,const char *name);
   int (*open)(VFSDentry *dentry,VFSFile *file);
} VFSINodeOperation;

typedef struct VFSFile{
   VFSDentry *dentry;
   u64 seek;
   VFSFileOperation *operation;
} VFSFile;

typedef struct VFSDentry{
   SpinLock lock;
   AtomicType ref;

   VFSDentryType type;
   VFSINode *inode;
   VFSDentry *parent;
   const char *name;

   FileSystemMount *mnt; 
      /*Only for type VFSDentryMount.*/

   ListHead children;
   ListHead list;
} VFSDentry;

typedef struct VFSINode{
   u64 start;
   u64 size;
   u64 inodeStart;
   BlockDevicePart *part;

   void *data;
   VFSINodeOperation *operation;
} VFSINode;

typedef struct FileSystemMount{
   VFSDentry *parent;
   VFSDentry *root;
} FileSystemMount;

typedef struct FileSystem{
   int (*mount)(BlockDevicePart *part,FileSystemMount *mnt);
   ListHead list;
   const char *name;
} FileSystem;

int registerFileSystem(FileSystem *system);
FileSystem *lookForFileSystem(const char *name);
BlockDevicePart *openBlockDeviceFile(const char *path);

int doMount(const char *point,FileSystem *fs,BlockDevicePart *part);
int doOpen(const char *path);
int doClose(int fd);
int doRead(int fd,void *buf,u64 size);
int writeFile(VFSFile *file,const void *buf,u64 size);
int doLSeek(int fd,u64 offset);

VFSFile *openFile(const char *path);
int readFile(VFSFile *file,void *buf,u64 size);
int doWrite(int fd,const void *buf,u64 size);
int closeFile(VFSFile *file);
int lseekFile(VFSFile *file,u64 offset);

VFSFile *cloneFile(VFSFile *file);
