#pragma once
#include <core/const.h>
#include <core/list.h>
#include <core/hlist.h>
#include <cpu/spinlock.h>
#include <cpu/atomic.h>
#include <task/semaphore.h>

typedef struct BlockDevicePart BlockDevicePart;
typedef struct VFSINode VFSINode;
typedef struct VFSFile VFSFile;
typedef struct VFSDentry VFSDentry;
typedef struct FileSystemMount FileSystemMount;
typedef struct FileSystem FileSystem;

typedef int (*VFSDirFiller)(void *data,u8 isDir,u64 length,const char *name);

typedef enum VFSDentryType{
   VFSDentryFile,
   VFSDentryDir,
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
   int (*readDir)(VFSFile *file,VFSDirFiller filler,void *data);
   int (*read)(VFSFile *file,void *buf,u64 size,u64 *seek);
   int (*write)(VFSFile *file,const void *buf,u64 size,u64 *seek);
   int (*lseek)(VFSFile *file,s64 offset,int type);
   int (*close)(VFSFile *file);
} VFSFileOperation;

typedef struct VFSINodeOperation
{
   int (*mkdir)(VFSDentry *dentry,const char *name);
   int (*unlink)(VFSDentry *result);
   int (*lookUp)(VFSDentry *dentry,VFSDentry *result,const char *name);
   int (*open)(VFSDentry *dentry,VFSFile *file,int mode);
} VFSINodeOperation;

typedef struct VFSFile{
   AtomicType ref;

   int mode;
   VFSDentry *dentry;
   u64 seek;
   void *data;
   VFSFileOperation *operation;
} VFSFile;

typedef struct VFSDentry{
   AtomicType ref;

   VFSDentryType type;
   VFSINode *inode;
   VFSDentry *parent;
   const char *name;
   u64 hash;

   FileSystemMount *mnt; 
   FileSystemMount *mounted;

   HashListNode node;
} VFSDentry;

typedef struct VFSINode{
   u64 start;
   u64 size;
   u64 inodeStart;
   BlockDevicePart *part;

   void *data;
   VFSINodeOperation *operation;
   Semaphore semaphore;
} VFSINode;

typedef struct FileSystemMount{
   VFSDentry *point;
   VFSDentry *root;

   FileSystem *fs;
   ListHead list;
} FileSystemMount;

typedef struct FileSystem{
   int (*mount)(BlockDevicePart *part,FileSystemMount *mnt);
   ListHead list;
   const char *name;

   ListHead mounts;
   SpinLock lock;
} FileSystem;

#define O_ACCMODE   0x0003 /*Access Mode.*/
#define O_RDONLY    0x0001 /*Read Only.*/
#define O_WRONLY    0x0002 /*Write Only.*/
#define O_RDWR      0x0003 /*Read And Write.*/
#define O_CLOEXEC   0x0010 /*Close On Exec.*/
#define O_DIRECTORY 0x0020 /*Must Be A Directory.*/

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int registerFileSystem(FileSystem *system);
FileSystem *lookForFileSystem(const char *name);
BlockDevicePart *openBlockDeviceFile(const char *path);

int doChroot(const char *dir);
int doMount(const char *point,FileSystem *fs,
          BlockDevicePart *part,u8 init);
int doOpen(const char *path,int mode);
int doClose(int fd);
int doRead(int fd,void *buf,u64 size);
int doLSeek(int fd,s64 offset,int type);
int doGetDents64(int fd,void *data,u64 size);
int doWrite(int fd,const void *buf,u64 size);
int doChdir(const char *dir);
int doGetCwd(char *buffer,u64 size);
int doDup(int fd);
int doDup2(int fd,int new);

VFSFile *vfsGetFile(VFSFile *file);
VFSFile *vfsPutFile(VFSFile *file);

VFSFile *openFile(const char *path,int mode);
int readFile(VFSFile *file,void *buf,u64 size);
int writeFile(VFSFile *file,const void *buf,u64 size);
int closeFile(VFSFile *file);
int lseekFile(VFSFile *file,s64 offset,int type);

int mountRoot(BlockDevicePart *part);
