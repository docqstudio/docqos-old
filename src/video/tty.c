#include <core/const.h>
#include <filesystem/virtual.h>
#include <filesystem/devfs.h>
#include <video/vesa.h>
#include <cpu/io.h>
#include <lib/string.h>

typedef struct TTYReader{
   Task *reader;
   u8 buf[50];
   u64 size;
   u64 pos;
   SpinLock lock;
} TTYReader;

static int ttyWrite(VFSFile *file,const void *buf,u64 size);
static int ttyRead(VFSFile *file,void *buf,u64 size);

static VFSFileOperation ttyFileOperation = {
   .lseek = 0,
   .read = ttyRead,
   .write = ttyWrite
};

static TTYReader ttyReader;

static int ttyWrite(VFSFile *file,const void *buf,u64 size)
{
   writeString((const char *)buf); /*Ingore size.*/
   return 0;
}

static int ttyRead(VFSFile *file,void *buf,u64 size)
{
   if(size <= 1)
      return -1;
   if(size >= sizeof(ttyReader.buf) / sizeof(ttyReader.buf[0]))
      return -1;
   Task *current = getCurrentTask();
   u64 rflags;
   lockSpinLockCloseInterrupt(&ttyReader.lock,&rflags);
   if(ttyReader.reader)
      goto failed; /*If there is a reader,failed!*/
   ttyReader.reader = current;
   ttyReader.size = size - 1;
   ttyReader.pos = 0;
   current->state = TaskStopping; /*Stop the current task.*/
   unlockSpinLockRestoreInterrupt(&ttyReader.lock,&rflags);
   schedule();
   u64 __size = ttyReader.pos;
   memcpy(buf,ttyReader.buf,__size + 1);
   ttyReader.reader = 0; /*I think there is no need to lock ttyReader.lock,right?*/
   return __size + 1;
failed:
   unlockSpinLockRestoreInterrupt(&ttyReader.lock,&rflags);
   return -1;
}

static int initTTY(void)
{
   ttyReader.reader = 0;
   ttyReader.pos = ttyReader.size = 0;
   initSpinLock(&ttyReader.lock);
   return devfsRegisterDevice(&ttyFileOperation,"tty");
      /*Register "/dev/tty" file.*/
}

int ttyKeyboardPress(char i)
{
   u64 rflags;
   char string[2] = {i,'\0'};
   lockSpinLockCloseInterrupt(&ttyReader.lock,&rflags);
   if(!ttyReader.reader) /*If no readers,just goto out.*/
      goto out;
   if(ttyReader.pos == ttyReader.size) /*If the buffer is full,goto out.*/
      goto out;
   if(i == '\n') 
      goto wakeUp; /*Wake up the reader.*/
   if(i == '\b')
   {
      if(ttyReader.pos)
         --ttyReader.pos;
      else
         goto ret;
      goto out;
   }
   ttyReader.buf[ttyReader.pos++] = i;
   if(ttyReader.pos == ttyReader.size) /*If the buffer is full,wake up the reader.*/
      goto wakeUp;
out:
   unlockSpinLockRestoreInterrupt(&ttyReader.lock,&rflags);
   writeString(string);
   return 0;
ret:
   unlockSpinLockRestoreInterrupt(&ttyReader.lock,&rflags);
   return 0;
wakeUp:
   ttyReader.buf[ttyReader.pos] = '\0'; /*Set end.*/
   unlockSpinLockRestoreInterrupt(&ttyReader.lock,&rflags);
   string[0] = i;
   if(i != '\n')
      writeString(string);
   string[0] = '\n';
   writeString(string);
   wakeUpTask(ttyReader.reader); /*Wake up the reader!*/
   return 0;
}

driverInitcall(initTTY);
