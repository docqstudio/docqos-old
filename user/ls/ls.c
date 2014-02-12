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
   int size,i = 8,first = 0;
   if((size = getdents64(fd,&buffer[8],sizeof(buffer) - 8)) < 0)
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
            buffer[i + 0] = 'f';
            buffer[i - 1] = 'f';
            buffer[i - 2] = '9';
            buffer[i - 3] = '9';
            buffer[i - 4] = '0';
            buffer[i - 5] = '0';
            buffer[i - 6] = 's';
            buffer[i - 7] = '\xff';
            write(stdout,&buffer[i - 7],0);
         }else{
            buffer[i + 1] = 'f';
            buffer[i + 0] = 'f';
            buffer[i - 1] = '9';
            buffer[i - 2] = '9'; /*Color Blue.*/
            buffer[i - 3] = '0';
            buffer[i - 4] = '0';
            buffer[i - 5] = 's';
            buffer[i - 6] = '\xff';
            write(stdout,&buffer[i - 6],0);
         }
         break;
      }
      write(stdout,"\t",0);
      i += length + 3;
   }
   close(fd);
   write(stdout,"\n",0); /*Print a '\n'.*/
   return 0;
}
