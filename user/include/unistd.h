#pragma once

#define stdin 0
#define stdout 1
#define stderr 2

int fork(void);
int exit(int n);
int open(const char *path);
int close(int fd);
int read(int fd,void *buf,unsigned long size);
int write(int fd,const void *buf,unsigned long size);
int execve(const char *path,const char *arg[],const char *envp[]);
int waitpid(int pid,int *result,int nowait);
