#include <core/const.h>
#include <interrupt/interrupt.h>
#include <cpu/io.h>
#include <cpu/spinlock.h>
#include <video/console.h>
#include <task/task.h>

#define KB_DATA   0x60
#define KB_STATUS 0x64

#define KB_STATUS_BUSY    0x2
#define KB_STATUS_DATA    0x1

#define KB_CMD_LED        0xed
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
   '-' ,'=' ,'\b','\t',
   'q' ,'w' ,'e' ,'r' ,
   't' ,'y' ,'u' ,'i' ,
   'o' ,'p' ,'[' ,']' ,
   '\n','\0','a' ,'s' ,
   'd' ,'f' ,'g' ,'h' ,
   'j' ,'k' ,'l' ,';' ,
   '\'','`' ,'\0','\\',
   'z' ,'x' ,'c' ,'v' ,
   'b' ,'n' ,'m' ,',' ,
   '.' ,'/' ,'\0','*' ,
   '\0',' ' ,'\0','\0',
   '\0','\0','\0','\0',
   '\0','\0','\0','\0',
   '\0','\0','\0','7' ,
   '8' ,'9' ,'-' ,'4' ,
   '5' ,'6' ,'+' ,'1' ,
   '2' ,'3' ,'0' ,'.'
};

static const u8 keyboardMapShift[] = {
   '\0','\0','!' ,'@' ,
   '#' ,'$' ,'%' ,'^' ,
   '&' ,'*' ,'(' ,')' ,
   '_' ,'+' ,'\b','\t',
   'q' ,'w' ,'e' ,'r' ,
   't' ,'y' ,'u' ,'i' ,
   'o' ,'p' ,'{' ,'}' ,
   '\n','\0','a' ,'s' ,
   'd' ,'f' ,'g' ,'h' ,
   'j' ,'k' ,'l' ,':' ,
   '\"','~' ,'\0','|' ,
   'z' ,'x' ,'c' ,'v' ,
   'b' ,'n' ,'m' ,'<' ,
   '>' ,'?' ,'\0','*' ,
   '\0',' ' ,'\0','\0',
   '\0','\0','\0','\0',
   '\0','\0','\0','\0',
   '\0','\0','\0','7' ,
   '8' ,'9' ,'-' ,'4' ,
   '5' ,'6' ,'+' ,'1' ,
   '2' ,'3' ,'0' ,'.'
};

static u8 keyboardBuffer[128];
static int keyboardRead = 0,keyboardWrite = 0;
static SpinLock keyboardLock;
static Task *keyboardTask;

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

static int keyboardTestIn(void)
{
   if(inb(KB_STATUS) & KB_STATUS_DATA)
      return (inb(KB_DATA),1);
   return 0;
}

static int keyboardIRQ(IRQRegisters *reg,void *unused)
{
   u64 rflags;
   u8 data;
   lockSpinLockCloseInterrupt(&keyboardLock,&rflags);
   while(inb(KB_STATUS) & KB_STATUS_DATA){
      data = inb(KB_DATA); /*Get scan code and put it into keyboardBuffer.*/
      keyboardBuffer[keyboardWrite++] = data;
      if(keyboardWrite == sizeof(keyboardBuffer)/sizeof(keyboardBuffer[0]))
         keyboardWrite = 0;
      inb(KB_STATUS);
   }
   wakeUpTask(keyboardTask);
   unlockSpinLockRestoreInterrupt(&keyboardLock,&rflags);
   return 0;
}

static int __keyboardTask(void *data)
{
   u8 shift = 0,caps = 0,num = 1,scroll = 0;
   keyboardTask = getCurrentTask();
   //keyboardOut(KB_CMD_LED);
   //keyboardOut((scroll << 0) | (num << 1) | (caps << 2));
   for(;;)
   {
      u64 rflags;
      u8 i,keypad = 0;
      lockSpinLockCloseInterrupt(&keyboardLock,&rflags);
      if(keyboardRead == keyboardWrite)
      {
         keyboardTask->state = TaskStopping;
         unlockSpinLockRestoreInterrupt(&keyboardLock,&rflags);
         schedule();
         continue;
      }
reget:
      i = keyboardBuffer[keyboardRead++];
      if(keyboardRead == sizeof(keyboardBuffer)/sizeof(keyboardBuffer[0]))
         keyboardRead = 0;
      if(i == 0xe0)
         goto reget;
      unlockSpinLockRestoreInterrupt(&keyboardLock,&rflags);
         /*Get scan code.*/

      u8 make = !(i & 0x80);
      i &= 0x7f;
      switch(i)
      {
      case 0x2a:
      case 0x36:
         shift = !!make;
         break;
      case 0x3a:
         if(!make)
            break;
         caps = !caps;
     //    keyboardOut(KB_CMD_LED);
     //    keyboardOut((scroll << 0) | (num << 1) | (caps << 2));
         break;
      case 0x45:
         if(!make)
            break;
         num = !num;
      //   keyboardOut(KB_CMD_LED);
      //   keyboardOut((scroll << 0) | (num << 1) | (caps << 2));
      case 0x46:
         if(!make)
            break;
         scroll = !scroll;
   //      keyboardOut(KB_CMD_LED);
   //      keyboardOut((scroll << 0) | (num << 1) | (caps << 2));
         break;
      default:
         if(!make)
            break;
         if(i >= sizeof(keyboardMap) / sizeof(keyboardMap[0]))
            break;
         if(0x47 <= i && i <= 0x53)
            keypad = 1;
         if(shift)
            i = keyboardMapShift[i];
         else
            i = keyboardMap[i];
         if(!i)
            break;
         if(('0' <= i) && (i <= '9') && keypad)
            if(!num)
               break;
         if(('a' <= i) && (i <= 'z'))
            if(shift ^ caps)
               i -= 'a' - 'A';
         ttyKeyboardPress(i); /*Tell tty.*/
         break;
      }
   }
   return 0;
}

static int initKeyboard(void)
{
   while(keyboardTestIn())
      asm volatile("pause;hlt");
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
      return -EIO; /*Exist,but failed to init it.*/

   keyboardRead = keyboardWrite = 0;
   keyboardTask = 0;
   initSpinLock(&keyboardLock);
   requestIRQ(KB_IRQ,&keyboardIRQ);
   createKernelTask(&__keyboardTask,0);
   return 0;
}

driverInitcall(initKeyboard);
