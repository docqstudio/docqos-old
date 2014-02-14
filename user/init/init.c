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

int shellCommandCd(const char *dir)
{
   if(!dir)
      return 0;
   int retval = chdir(dir);
   if(!retval)
      return 0;
   write(stdout,"No such dir or file!\n",0);
                 /*Maybe no such dir or file.*/
                 /*But it can also be a file.*/
   return retval;
}

int shellRunCommand(char *cmd)
{
   while(*cmd == ' ')
      ++cmd; /*Skip ' '.*/
   if(*cmd == '\0')
      return 0;
   /*Child process.*/
   const char *argv[10];
   int i = 0;
   while(*cmd && i + 1 < sizeof(argv) / sizeof(argv[0]))
      argv[i++] = getCommandArgument(&cmd);
   argv[i] = 0; /*Set end.*/
  
   if(argv[0][0] == 'c' && argv[0][1] == 'd' && argv[0][2] == '\0')
      return shellCommandCd(argv[1]); /*The 'cd' commmad.*/

   int ret = 0;
   int pid = fork();
   if(pid < 0) /*Fail to fork.*/
      return write(stdout,"Can't fork.Abort!",0);
   else if(pid > 0) /*Parent process.*/
      return (waitpid(pid,&ret,0),ret); /*Wait for the child process.*/

   execve(argv[0],argv,0);
   write(stdout,"Bad Command!!!\n",0);
            /*Can't execve?Maybe this is a bad command.*/
   exit(0);
   return 0;
}

int shell(void)
{
   char cwd[64] = "\xffsff0000losthost \xffs0055ff/ $ ";
   volatile char cmd[35];
   int i;
   for(;;)
   {    /*Print "losthost $(getcwd) # ".*/
      i = getcwd(cwd + 25,sizeof(cwd) - 25 - 4);
                          /*Get the current working dir.*/
      cwd[25 + i] = ' ';
      cwd[26 + i] = '#';
      cwd[27 + i] = ' ';
      cwd[28 + i] = '\0';
      write(stdout,cwd,0);
      read(stdin,(void *)cmd,sizeof(cmd) - 3);

      if(cmd[0] == 'r' && cmd[1] == 'e' && cmd[2] == 'b' &&
         cmd[3] == 'o' && cmd[4] == 'o' && cmd[5] == 't' &&
         cmd[6] == '\0') /*Reboot command.*/
         reboot(REBOOT_REBOOT_COMMAND);
      if(cmd[0] == 's' && cmd[1] == 'h' && cmd[2] == 'u' &&
         cmd[3] == 't' && cmd[4] == 'd' && cmd[5] == 'o' &&
         cmd[6] == 'w' && cmd[7] == 'n' && cmd[8] == '\0') /*Shutdown command.*/
         reboot(REBOOT_POWEROFF_COMMAND);
      shellRunCommand((char *)cmd); /*Try to run this command.*/
   }
   write(stdout,"Goodbye!\n",0);
   return 0;
}
