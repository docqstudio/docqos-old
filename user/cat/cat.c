#include <unistd.h>
#include <errno.h>
#include <string.h>

int main(int argc,const char *argv[])
{
   char buf[129];
   long size = 0;
   char null = 0;
   int fd = stdin; /*If no arguments,just cat stdin.*/
   if(argc > 1)
      fd = open(argv[argc - 1]);
   if(fd < 0)
   {
      write(stdout,strerror(errno),0);
         /*Get the error string.*/
      write(stdout,"\n",0);
      return 0;
   }
   for(;;)
   {
      size = read(fd,buf,sizeof(buf) - 1);
      if(size <= 0)
         break; /*No data,return.*/
      ++null;
      buf[size] = '\0';
      write(stdout,buf,size); /*Write to stdout.*/
   }
   if(size < 0)
      write(stdout,strerror(errno),0),write(stdout,"\n",0);
   if(fd != stdin)
      close(fd); /*Close the file.*/
   if(null)
      write(stdout,"\n",0);
   return 0;
}
