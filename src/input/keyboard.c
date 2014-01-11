#include <core/const.h>
#include <interrupt/interrupt.h>
#include <cpu/io.h>
#include <video/console.h>
#include <task/semaphore.h>

#define KB_DATA   0x60
#define KB_STATUS 0x64

#define KB_STATUS_BUSY    0x2
#define KB_STATUS_DATA    0x1

#define KB_CMD_IDENTIFY   0xf2
#define KB_CMD_ESCAN      0xf4
#define KB_CMD_DSCAN      0xf5

#define KB_DATA_ACK       0xfa

#define KB_IRQ            0x01

extern int ttyKeyboardPress(char i);

static const u8 keyboardMap[] = {
   '\0','\0','1' ,'2',
   '3' ,'4' ,'5' ,'6' ,
   '7' ,'8' ,'9' ,'0' ,
   '-' ,'=' ,'\0','\t',
   'q' ,'w' ,'e' ,'r' ,
   't' ,'y' ,'u' ,'i' ,
   'o' ,'p' ,'[' ,']' ,
   '\n','\0','a' ,'s' ,
   'd' ,'f' ,'g' ,'h' ,
   'j' ,'k' ,'l' ,';' ,
   '\'','`' ,'\0','\\',
   'z' ,'x' ,'c' ,'v' ,
   'b' ,'n' ,'m' ,',' ,
   '.' ,'/' ,'\0','\0',
   '\0',' ' ,'\0','\0' /*A very simple keyboard map.*/
};

static u8 keyboardBuffer[128];
static int keyboardRead = 0,keyboardWrite = 0;
static SpinLock keyboardLock;
static Semaphore keyboardSemaphore;

static int keyboardOut(int data)
{
   while(inb(KB_STATUS) & KB_STATUS_BUSY)
      asm volatile("pause;hlt");
   outb(KB_DATA,data);
   while((inb(KB_STATUS) & KB_STATUS_DATA) == 0)
      asm volatile("pause;hlt"); /*Wait for data.*/
   return inb(KB_DATA);
}

static int keyboardIn(void)
{
   while((inb(KB_STATUS) & KB_STATUS_DATA) == 0)
      asm volatile("pause;hlt"); /*Wait for data.*/
   return inb(KB_DATA);
}

static int keyboardIRQ(IRQRegisters *reg,void *data)
{
   u64 rflags;
   u8 i = inb(KB_DATA); /*Get scan code and put it into keyboardBuffer.*/
   lockSpinLockDisableInterrupt(&keyboardLock,&rflags);
   keyboardBuffer[keyboardWrite++] = i;
   if(keyboardWrite == sizeof(keyboardBuffer)/sizeof(keyboardBuffer[0]))
      keyboardWrite = 0;
   unlockSpinLockRestoreInterrupt(&keyboardLock,&rflags);
   upSemaphore(&keyboardSemaphore); /*Wake up keyboardTask.*/
   return 0;
}

static int keyboardTask(void *data)
{
   downSemaphore(&keyboardSemaphore);
   for(;;)
   {
      downSemaphore(&keyboardSemaphore); /*See also keyboardIRQ.*/
      u64 rflags;
      u8 i;
      lockSpinLockDisableInterrupt(&keyboardLock,&rflags);
      i = keyboardBuffer[keyboardRead++];
      if(keyboardRead == sizeof(keyboardBuffer)/sizeof(keyboardBuffer[0]))
         keyboardRead = 0;
      unlockSpinLockRestoreInterrupt(&keyboardLock,&rflags);
         /*Get scan code.*/

      if(i & 0x80)
         continue; /*Ingnore break code.*/
      if(i < sizeof(keyboardMap)/sizeof(keyboardMap[0]))
         if(keyboardMap[i] != '\0') /*Can print?*/
            ttyKeyboardPress(keyboardMap[i]); /*Tell tty.*/
   }
   return 0;
}

static int initKeyboard(void)
{
   if(keyboardOut(KB_CMD_DSCAN) != KB_DATA_ACK)
      return 0;
   if(keyboardOut(KB_CMD_IDENTIFY) != KB_DATA_ACK)
      return 0;
   u8 byte = keyboardIn();
   if(byte == 0xab)
      keyboardIn();
   else
      return 0;
   /*Now we are sure that PS/2 keyboard is existed.*/
   /*Only check the first PS/2 port now.*/
   if(keyboardOut(KB_CMD_ESCAN) != KB_DATA_ACK)
      return -1; /*Exist,but failed to init it.*/

   initSpinLock(&keyboardLock);
   initSemaphore(&keyboardSemaphore);
   requestIRQ(KB_IRQ,&keyboardIRQ);
   createKernelTask(&keyboardTask,0);
   return 0;
}

driverInitcall(initKeyboard);
