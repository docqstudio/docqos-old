#pragma once
#include <core/const.h>
#include <core/list.h>
#include <cpu/spinlock.h>
#include <cpu/atomic.h>

typedef struct BlockDevicePart BlockDevicePart;
typedef struct VFSINode VFSINode;
typedef struct VFSFile VFSFile;
typedef struct VFSDentry VFSDentry;

typedef enum VFSDentryType{
   VFSDentryFile,
   VFSDentryDir,
   VFSDentryMount
} VFSDentryType;

typedef struct VFSFileOperation
{
   int (*read)(VFSFile *file,void *buf,u64 size);
   int (*write)(VFSFile *file,void *buf,u64 size);
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

   ListHead children;
   ListHead list;
} VFSDentry;

typedef struct VFSINode{
   u64 start;
   u64 size;
   u64 inodeStart;
   BlockDevicePart *part;

   VFSINodeOperation *operation;
} VFSINode;

typedef struct FileSystemMount{
   VFSDentry *parent;
   VFSDentry *root;
} FileSystemMount;

typedef struct FileSystem{
   int (*mount)(BlockDevicePart *part,FileSystemMount *mnt);
   ListHead list;
} FileSystem;

int registerFileSystem(FileSystem *system);

int doMount(const char *point,BlockDevicePart *part);
int doOpen(const char *path);
int doClose(int fd);
int doRead(int fd,void *buf,u64 size);
