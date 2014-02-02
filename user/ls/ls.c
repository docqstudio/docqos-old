#include <unistd.h>

int main(int argc,const char *argv[])
{
   const char *dir;
   if(argc > 1)
      dir = argv[1];
   else
      dir = ".";
   int fd = open(dir);
   if(fd < 0)
      return write(stdout,"Can't open the dir!\n",0);
   unsigned char buffer[2048 + 5];
   int size,i = 5,first = 0;
   if((size = getdents64(fd,&buffer[5],sizeof(buffer) - 5)) < 0)
      return (close(fd),write(stdout,"Can't get the data!\n",0));
   while(i < size + 5)
   {
      if(first == 5){
         write(stdout,"\n",0);
         first = 0;
      }
      unsigned char isdir = !!buffer[i]; /*Is it a dir?*/
      unsigned char length = buffer[i + 1];
      switch(isdir)
      {
      case 0:
         if(first++){
            buffer[i + 1] = ' '; /*Add a ' ' before filename.*/
            write(stdout,&buffer[i + 1],0);
         }else{
            write(stdout,&buffer[i + 2],0);
         }
         break;
      case 1:
         if(first++){
            buffer[i + 1] = ' ';
            buffer[i + 0] = '\xff';
            buffer[i - 1] = '\x99';
            buffer[i - 2] = '\x00'; /*Color Blue and ' '.*/
            buffer[i - 3] = 's';
            buffer[i - 4] = '\xff';
            write(stdout,&buffer[i - 4],0);
         }else{
            buffer[i + 1] = '\xff';
            buffer[i + 0] = '\x99';
            buffer[i - 1] = '\x00';
            buffer[i - 2] = 's'; /*Color Blue.*/
            buffer[i - 3] = '\xff';
            write(stdout,&buffer[i - 3],0);
         }
         break;
      }
      write(stdout,"\t",0);
      i += length + 3;
   }
   write(stdout,"\n",0); /*Print a '\n'.*/
   return 0;
}
