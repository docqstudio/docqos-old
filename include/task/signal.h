#pragma once
#include <core/const.h>
#include <cpu/spinlock.h>
#include <cpu/atomic.h>
#include <interrupt/interrupt.h>

typedef unsigned int SignalInformation; /*It is unused,ingore it now.*/
typedef struct SignalSet
{
   unsigned long value[1];
} SignalSet;

typedef struct SignalAction
{
   /*Kernel Space              User Space*/
   int (*handler)(int sig);  /*void (*sa_handler)(int sig);*/
   int (*sigaction)(int sig,SignalInformation *siginfo,void *data);
                             /*void (*sa_sigaction)(int,siginfo_t *,void *);*/
   SignalSet mask;           /*sigset_t sa_mask;*/
   int flags;                /*int sa_flags;*/
   int (*restorer)(void);    /*void (*sa_restorer)(void);*/
               /*The restorer field will be unused in the future.*/
               /*But now we use it as a function that will call sigret system call*/
               /*to restore the content.*/
} SignalAction;

typedef struct TaskSignal
{
   SpinLock lock;
   SignalAction actions[64];
   AtomicType ref;
} TaskSignal;

typedef struct SignalContent
{
   u64 rax,rbx,rcx,rdx,rsi,rdi;
   u64 r8,r9,r10,r11,r12,r13,r14,r15;
   u64 rip,blocked;
} SignalContent;

#define SIG_DFL         ((void *)0ul)
#define SIG_IGN         ((void *)1ul)

#define SIGHUP          1       /* Hangup (POSIX).  */
#define SIGINT          2       /* Interrupt (ANSI).  */
#define SIGQUIT         3       /* Quit (POSIX).  */
#define SIGILL          4       /* Illegal instruction (ANSI).  */
#define SIGTRAP         5       /* Trace trap (POSIX).  */
#define SIGABRT         6       /* Abort (ANSI).  */
#define SIGIOT          6       /* IOT trap (4.2 BSD).  */
#define SIGBUS          7       /* BUS error (4.2 BSD).  */
#define SIGFPE          8       /* Floating-point exception (ANSI).  */
#define SIGKILL         9       /* Kill, unblockable (POSIX).  */
#define SIGUSR1         10      /* User-defined signal 1 (POSIX).  */
#define SIGSEGV         11      /* Segmentation violation (ANSI).  */
#define SIGUSR2         12      /* User-defined signal 2 (POSIX).  */
#define SIGPIPE         13      /* Broken pipe (POSIX).  */
#define SIGALRM         14      /* Alarm clock (POSIX).  */
#define SIGTERM         15      /* Termination (ANSI).  */
#define SIGSTKFLT       16      /* Stack fault.  */
#define SIGCLD          SIGCHLD /* Same as SIGCHLD (System V).  */
#define SIGCHLD         17      /* Child status has changed (POSIX).  */
#define SIGCONT         18      /* Continue (POSIX).  */
#define SIGSTOP         19      /* Stop, unblockable (POSIX).  */
#define SIGTSTP         20      /* Keyboard stop (POSIX).  */
#define SIGTTIN         21      /* Background read from tty (POSIX).  */
#define SIGTTOU         22      /* Background write to tty (POSIX).  */
#define SIGURG          23      /* Urgent condition on socket (4.2 BSD).  */
#define SIGXCPU         24      /* CPU limit exceeded (4.2 BSD).  */
#define SIGXFSZ         25      /* File size limit exceeded (4.2 BSD).  */
#define SIGVTALRM       26      /* Virtual alarm clock (4.2 BSD).  */
#define SIGPROF         27      /* Profiling alarm clock (4.2 BSD).  */
#define SIGWINCH        28      /* Window size change (4.3 BSD, Sun).  */
#define SIGPOLL         SIGIO   /* Pollable event occurred (System V).  */
#define SIGIO           29      /* I/O now possible (4.2 BSD).  */
#define SIGPWR          30      /* Power failure restart (System V).  */
#define SIGSYS          31      /* Bad system call.  */
#define SIGUNUSED       31

int doSignalAction(int sig,SignalAction *action,SignalAction *unused);
int doKill(unsigned int pid,unsigned int sig);
int doSignalReturn(IRQRegisters *reg);
int handleSignal(IRQRegisters *reg);

inline int taskSignalPending(Task *task) __attribute__ ((always_inline));
inline int handleSignalCheck(IRQRegisters *reg) __attribute__ ((always_inline));

inline int taskSignalPending(Task *task)
{
   return task->pending & ~task->blocked;
}

inline int handleSignalCheck(IRQRegisters *reg)
{
   return ((reg->cs & 3) == 3) && taskSignalPending(getCurrentTask());
}
