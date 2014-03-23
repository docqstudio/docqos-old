#include <unistd.h>
#include <errno.h>
#include <string.h>

int main(int argc,const char *argv[])
{
   const char *dir;
   if(argc > 1)
      dir = argv[1];
   else
      dir = ".";
   int fd = open(dir,O_RDONLY | O_DIRECTORY);
   if(fd < 0)
   {
      write(stdout,strerror(errno),0);
      write(stdout,"\n",0);
      return -1;
   }
   unsigned char buffer[2048 + 5];
   int size,i = 8,first = 0;
   if((size = getdents64(fd,&buffer[8],sizeof(buffer) - 8)) < 0)
   {
      write(stdout,strerror(errno),0);
         /*Write the error string to the screen.*/
      write(stdout,"\n",0);
      return -1;
   }
retry:
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
            buffer[i + 0] = 'm';
            buffer[i - 1] = '4';
            buffer[i - 2] = '3';
            buffer[i - 3] = ';';
            buffer[i - 4] = '1';
            buffer[i - 5] = '0';
            buffer[i - 6] = '[';
            buffer[i - 7] = '\033';
            write(stdout,&buffer[i - 7],0);
         }else{
            buffer[i + 1] = 'm';
            buffer[i + 0] = '4';
            buffer[i - 1] = '3';
            buffer[i - 2] = ';'; /*Color Blue.*/
            buffer[i - 3] = '1';
            buffer[i - 4] = '0';
            buffer[i - 5] = '[';
            buffer[i - 6] = '\033';
            write(stdout,&buffer[i - 6],0);
         }
         break;
      }
      write(stdout,"\t",0);
      i += length + 3;
   }
   if((size = getdents64(fd,&buffer[8],sizeof(buffer) - 8)) > 0 &&
      (i = 8))
      goto retry; /*Read until there is no data.*/
   close(fd);
   write(stdout,"\n",0); /*Print a '\n'.*/
   return 0;
}
