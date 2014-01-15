#include <unistd.h>

int shell(void);

int main(void)
{
   { /*Try to open stdin and stdout.*/
      int fd1 = open("/dev/tty");
      int fd2 = open("/dev/tty");
      if(fd2 < 0)
      { /*Fail to open stdout.*/
         if(fd1 > 0)
            close(fd1);
         return -1;
      }
      if(fd1 != stdin || fd2 != stdout)
      {
         write(fd2,"/sbin/init process failed:\n   "
                  "Can't open /dev/tty as stdout or stdin.\n",0);
         if(fd1 > 0)
            close(fd1);
         close(fd2);
         return -1;
      }
   } /*Now we have opened stdin and stdout.*/
   write(stdout,"Welcome to DOCQ OS.\n",0);
   shell();
   close(stdin);
   close(stdout);
   return 0;
}

const char *getCommandArgument(char **cmd)
{
   char *retval = *cmd;
   char *tmp = *cmd;
   while(*tmp != ' ' && *tmp != '\0')
      ++tmp;    /*Skip arguments.*/
   if(*tmp == '\0')
      goto out;
   *tmp++ = '\0'; /*Set end.*/
   while(*tmp == ' ') /*Skip ' '.*/
      ++tmp;
out:
   *cmd = tmp;
   return retval;
}

int shellRunCommand(char *cmd)
{
   while(*cmd == ' ')
      ++cmd; /*Skip ' '.*/
   if(*cmd == '\0')
      return 0;
   
   int pid = fork();
   int ret = 0;
   if(pid < 0) /*Fail to fork.*/
      return write(stdout,"Can't fork.Abort!",0);
   else if(pid > 0) /*Parent process.*/
      return (waitpid(pid,&ret,0),ret); /*Wait for the child process.*/
   /*Child process.*/
   const char *argv[10];
   int i = 0;
   while(*cmd && i + 1 < sizeof(argv) / sizeof(argv[0]))
      argv[i++] = getCommandArgument(&cmd);
   argv[i] = 0; /*Set end.*/

   execve(argv[0],argv,0);
   write(stdout,"Bad Command!!!\n",0);
            /*Can't execve?Maybe this is a bad command.*/
   exit(0);
   return 0;
}

int shell(void)
{
   volatile char cmd[35];
   for(;;)
   {    /*Print "losthost / $ ".*/
      write(stdout,"\xffs\xff\x00\x00losthost \xffs\x00\x00\xff/ $ ",0);
      read(stdin,(void *)cmd,sizeof(cmd) - 3);
      if(cmd[0] == 'e' && cmd[1] == 'x' && cmd[2] == 'i' &&
         cmd[3] == 't' && cmd[4] == '\0') /*Exit command.*/
         break;
      shellRunCommand((char *)cmd); /*Try to run this command.*/
   }
   write(stdout,"Goodbye!\n",0);
   return 0;
}
