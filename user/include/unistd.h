#pragma once

#define stdin 0
#define stdout 1
#define stderr 2

#define REBOOT_REBOOT_COMMAND    0xacde147525474417ul
#define REBOOT_POWEROFF_COMMAND  0x1234aeda78965421ul

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
