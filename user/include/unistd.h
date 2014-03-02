#pragma once

#ifndef stdin
#define stdin 0
#endif

#ifndef stdout
#define stdout 1
#endif

#ifndef stderr
#define stderr 2
#endif

/*Maybe these are defined in stdio.*/
/*If it happens,we don't define them!!*/

#define REBOOT_REBOOT_COMMAND    0xacde147525474417ul
#define REBOOT_POWEROFF_COMMAND  0x1234aeda78965421ul

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int fork(void);
int exit(int n);
int open(const char *path);
int close(int fd);
unsigned long read(int fd,void *buf,unsigned long size);
unsigned long write(int fd,const void *buf,unsigned long size);
int execve(const char *path,const char *arg[],const char *envp[]);
int waitpid(int pid,int *result,int nowait);
int reboot(unsigned long command);
int getpid(void);
int gettimeofday(unsigned long *time,void *unused);
unsigned long getdents64(int fd,void *buf,unsigned long size);
int chdir(const char *dir);
int getcwd(char *buffer,unsigned long size);
unsigned long lseek(int fd,signed long offset,int type);

int dup(int fd);
int dup2(int fd,int new);
