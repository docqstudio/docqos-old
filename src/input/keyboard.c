#include <core/const.h>
#include <interrupt/interrupt.h>
#include <cpu/io.h>
#include <cpu/spinlock.h>
#include <video/console.h>
#include <video/tty.h>
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
#define KB_DATA_RESEND    0xfe

#define KB_IRQ            0x01

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

static volatile int status = 0;

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

static int keyboardSetLED(u8 data)
{
   while(inb(KB_STATUS) & KB_STATUS_BUSY)
      asm volatile("pause");
   status = 0; /*Reset the status.*/
   outb(KB_DATA,KB_CMD_LED);
   while(status == 0) /*Wait until it changes.*/
      asm volatile("hlt");

   while(inb(KB_STATUS) & KB_STATUS_BUSY)
      asm volatile("pause");
   status = 0;
   outb(KB_DATA,data); /*Write the data to the data port.*/
   while(status == 0)
      asm volatile("hlt");
   return 0;
}

static int keyboardTestIn(void)
{
   if(inb(KB_STATUS) & KB_STATUS_DATA)
      return (inb(KB_DATA),1);
   return 0;
}

static u8 keyboardCallback(u8 data,KeyboardState *state)
{
   u8 keypad = 0;
   if(data == 0xe0 && (state->data = 1))
      return 0;

   u8 make = !(data & 0x80);
   data &= 0x7f;

   if(state->data && ((state->data = 0) || 1))
      switch(data)
      {
      case 0x48:
         return make ? KEY_UP : 0;
      case 0x50:
         return make ? KEY_DOWN : 0;
      case 0x4d:
         return make ? KEY_RIGHT : 0;
      case 0x4b:
         return make ? KEY_LEFT : 0;
      default:
         return 0; /*Ingore.*/
      }

   switch(data)
   {
   case 0x2a:
   case 0x36: /*Right-Shift and Left-Shift.*/
      state->shift = !!make;
      data = 0;
      break;
   case 0x1d: /*Right-Ctrl and Left-Ctrl.*/
      state->ctrl = !!make;
      data = 0;
      break;
   case 0x3a: /*CapsLock.*/
      if(!make)
         break;
      state->caps = !state->caps;
      keyboardSetLED((state->scroll << 0) | (state->num << 1) | (state->caps << 2));
      data = 0;
      break;
   case 0x45: /*NumLock.*/
      if(!make)
         break;
      state->num = !state->num;
      keyboardSetLED((state->scroll << 0) | (state->num << 1) | (state->caps << 2));
      data = 0;
      break;
   case 0x46: /*ScrollLock.*/
      if(!make)
         break;
      state->scroll = !state->scroll;
      keyboardSetLED((state->scroll << 0) | (state->num << 1) | (state->caps << 2));
      data = 0;
      break;
   default:
      if(!make && ((data = 0),1)) /*Ingore break code.*/
         break;
      if(data >= sizeof(keyboardMap) / sizeof(keyboardMap[0]) && 
                                              ((data = 0),1))
         break; 
      if(0x47 <= data && data <= 0x53)
         keypad = 1; /*Keypad.*/
      if(state->shift)
         data = keyboardMapShift[data];
      else
         data = keyboardMap[data];
      if(!data)
         break;
      if(('0' <= data) && (data <= '9') && keypad)
         if(!state->num) 
            break;
      if(('a' <= data) && (data <= 'z'))
         if(state->shift ^ state->caps)
            data -= 'a' - 'A'; /*To the supper case.*/
      return data;
   }
   return 0;
}


static int keyboardIRQ(IRQRegisters *reg,void *unused)
{
   u8 data;
   while(inb(KB_STATUS) & KB_STATUS_DATA){
      data = inb(KB_DATA); /*Get scan code and put it into keyboardBuffer.*/
      if((data == KB_DATA_ACK || data == KB_DATA_RESEND))
         status = data; /*Received ACK or RESEND!!*/
      else
         ttyKeyboardPress(&keyboardCallback,data);
      inb(KB_STATUS);
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

   requestIRQ(KB_IRQ,&keyboardIRQ);
   keyboardSetLED(0x2);
      /*Init the keyboard LED status.*/
   return 0;
}

driverInitcall(initKeyboard);
