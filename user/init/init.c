#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

void sigint(int sig)
{
   write(stdout,(sig == SIGINT) ?  "SIGINT Signal!" : "Unkonw Signal,BUG?",0);
   return (void)0;
}

int shell(void);

int main(int argc,char *argv[])
{
   if(argc == 0) /*Run from kernel.*/
   { /*Try to open stdin and stdout.*/
      int fd1 = open("/dev/tty",O_RDWR),fd2,fd3;
      if(fd1 < 0)
         return -1;
      if(fd1 != 0)
         return write(fd1,"INIT Process: \n"
         "      Can't open /dev/tty as stdin.\n",0);
      fd2 = dup2(fd1,1);
      fd3 = dup2(fd1,2);
      if(fd2 != 1 || fd3 != 2)
         return write(fd1,"INIT Process: \n"
         "      Can't dup stdin as stdout and stderr.\n",0);
      /*Now we have opened stdin and stdout,stderr.*/
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

   {
      struct sigaction action = 
      {
         .sa_handler = &sigint,
         .sa_mask = {{(1 << SIGINT)}}
      };
      sigaction(SIGINT,&action,0);
   }
   {
      struct sigaction action = 
      {
         .sa_handler = SIG_IGN,
         .sa_mask = {{(1 << SIGQUIT)}}
      };
      sigaction(SIGQUIT,&action,0);
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
   while(*tmp != ' ' && *tmp != '\n')
      ++tmp;    /*Skip arguments.*/
   if(*tmp == '\n' && ((*tmp = '\0') || 1))
      goto out;
   *tmp++ = '\0'; /*Set end.*/
   while(*tmp == ' ') /*Skip ' '.*/
      ++tmp;
   if(*tmp == '\n')
      *tmp = '\0';
out:
   *cmd = tmp;
   return retval;
}

int shellCommandCd(const char *dir)
{
   if(!dir)
      return 0;
   int retval = chdir(dir);
   if(retval != -1)
      return 0;
   write(stdout,strerror(errno),0);
           /*The error string.*/
   write(stdout,"\n",0);
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

int doTheTest(void)
{ /*Do some tests about getUserX putUserX .*/
   int retval = read(stdin,(void *)(~0ul - 7),5);
   write(stdout,"\n",0);
   if(retval == -1) /*Test the verifyUserAddress function.*/
      write(stdout,strerror(errno),0);
   write(stdout,"\n",0);

   retval = read(stdin,(void *)370ul,777);
   write(stdout,"\n",0);
   if(retval == -1) /*Test the exception fix up table.*/
      write(stdout,strerror(errno),0);
   write(stdout,"\n",0);
   return 0;
}

int shellRunCommand(char *cmd)
{
   static char pathenv[][64] = {
      "/bin/",
      "/sbin/"
   };

   if(cmd[0] == 't' && cmd[1] == 'e' && cmd[2] == 's' && cmd[3] == 't' && 
          (cmd[4] == ' ' || cmd[4] == '\0' || cmd[4] == '\n'))
     return doTheTest(); /*The 'test' command.*/

   while(*cmd == ' ')
      ++cmd; /*Skip ' '.*/
   if(*cmd == '\0' || *cmd == '\n')
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
   {
      waitpid(pid,&ret,0);
      pid = getpid();
      ioctl(stdin,TIOCSPGRP,&pid);
      return ret;
   }

   int fd;
   pid = getpid();
   ioctl(stdin,TIOCSPGRP,&pid);
   if(!shellCommandNeedLookForPath(argv[0]))
      fd = open(argv[0],O_RDONLY);
   else for(int i = 0;i < sizeof(pathenv) / sizeof(pathenv[0]);++i)
   {
      int j = 0;
      do{
         pathenv[i][j + 5 + i] = argv[0][j]; /*Copy it!*/
      }while(argv[0][++j] != 0);
      fd = open(pathenv[i],O_RDONLY); /*Exists?*/
      if(fd >= 0 && (argv[0] = pathenv[i]))
         break;
   }
   if(fd < 0)
      goto out;
   close(fd);

   execve(argv[0],argv,0);
out:
   write(stdout,"Bad Command: ",0);
   write(stdout,strerror(errno),0);
   write(stdout,"\n",0);
            /*Can't execve?Maybe this is a bad command.*/
   exit(0);
   return 0;
}

int shell(void)
{
   char cwd[64] = "\033[01;32mlocalhost \033[01;34m/ $";
   volatile char cmd[35];
   int i = getpid();
   ioctl(stdin,TIOCSPGRP,&i);
   for(;;)
   {    /*Print "localhost $(getcwd) # ".*/
      i = getcwd(cwd + 26,sizeof(cwd) - 26 - 4);
                          /*Get the current working dir.*/
      cwd[26 + i] = ' ';
      cwd[27 + i] = '#';
      cwd[28 + i] = ' ';
      cwd[29 + i] = '\0';
      write(stdout,cwd,0);
      i = read(stdin,(void *)cmd,sizeof(cmd) - 3);
      
      if(i > 0) /*EOF?*/
         shellRunCommand((char *)cmd); /*Try to run this command.*/\
      else
         write(stdout,"\n",0);
   }
   write(stdout,"Goodbye!\n",0);
   return 0;
}
