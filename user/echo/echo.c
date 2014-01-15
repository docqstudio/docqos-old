#include <unistd.h>

int main(int argc,const char *argv[])
{
   if(argc <= 1) /*If no arguments,exit.*/
      return 0;
   for(int i = 1;i < argc;++i)
   {
      write(stdout,argv[i],0);
      write(stdout," ",0); /*Write the arguments to stdout.(/dev/tty)*/
   }
   write(stdout,"\n",0); /*Write '\n'.*/
   return 0;
}
