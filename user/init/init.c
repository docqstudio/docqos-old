#include <unistd.h>

int shell(void);

int main(int argc,char *argv[])
{
   if(argc == 0) /*Run from kernel.*/
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
      }/*Now we have opened stdin and stdout.*/
   }else { /*Run from user.*/
      if(argc != 2)
         goto usage;
      if(argv[1][0] == '\0' || argv[1][1] != '\0')
         goto usage;
      if(argv[1][0] < '0' || argv[1][0] > '6')
         goto usage;
      int mode = argv[1][0] - '0';
      static char information[] = "Switching to Run Level 0.\n";
      information[23] = argv[1][0];
      write(stdout,information,0);
      if(mode > 0 && mode < 6)
         exit(0); /*Just do nothing.*/
      switch(mode)
      {
      case 0: /*Halt the system.*/
         write(stdout,"Halting......\n",0);
         reboot(REBOOT_POWEROFF_COMMAND);
         break;
      case 6: /*Reboot the system.*/
         write(stdout,"Rebooting......\n",0);
         reboot(REBOOT_REBOOT_COMMAND);
         break;
      default: /*It should never arrive here.*/
         goto usage;
      }
      exit(0);
usage:
      write(stdout,"Usage: init [Run Level]\n",0);
      write(stdout,"      Run Level: The run level you want to switch.\n",0);
      write(stdout,"                 Now we support 0 - 6.\n",0);
      write(stdout,"                 0:Halt,6:Reboot,1-5:Nothing.\n",0);
      exit(-1);
   }
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

int shellCommandNeedLookForPath(const char *path)
{
   char c;
   while((c = *path++) != '\0')
      if(c == '/')
         return 0; /*No need.*/
   return 1; /*Need!!*/
}

int shellRunCommand(char *cmd)
{
   static char pathenv[][64] = {
      "/bin/",
      "/sbin/"
   };

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

   int fd;
   if(!shellCommandNeedLookForPath(argv[0]))
      fd = open(argv[0]);
   else for(int i = 0;i < sizeof(pathenv) / sizeof(pathenv[0]);++i)
   {
      int j = 0;
      do{
         pathenv[i][j + 5 + i] = argv[0][j]; /*Copy it!*/
      }while(argv[0][++j] != 0);
      fd = open(pathenv[i]); /*Exists?*/
      if(fd >= 0 && (argv[0] = pathenv[i]))
         break;
   }
   if(fd < 0)
      goto out;
   close(fd);

   execve(argv[0],argv,0);
out:
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
      
      shellRunCommand((char *)cmd); /*Try to run this command.*/
   }
   write(stdout,"Goodbye!\n",0);
   return 0;
}
