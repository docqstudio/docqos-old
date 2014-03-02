#pragma once
#include <stdarg.h>

typedef int FILE;
extern FILE * const __stdout;
extern FILE * const __stdin;
extern FILE * const __stderr;

#ifdef stdout
#undef stdout
#endif

#ifdef stdin
#undef stdin
#endif

#ifdef stderr
#undef stderr
#endif
/*Maybe these have defined in unistd.h.*/
/*If it happens,undefine them.*/

#define stdout __stdout
#define stdin __stdin
#define stderr __stderr

extern FILE *fopen(const char *path,const char *mode);
extern char *fgets(char *buf,unsigned long size,FILE *file);
extern int fputs(const char *buf,FILE *file);
inline int puts(const char *buf);
inline char *gets(char *buf);

extern int printf(const char *format,...);
extern int fprintf(FILE *file,const char *format,...);
extern int sprintf(char *string,const char *format,...);

extern int vprintf(const char *format,va_list list);
extern int vfprintf(FILE *file,const char *format,va_list list);
extern int vsprintf(char *string,const char *format,va_list list);

extern int scanf(const char *format,...);
extern int fscanf(FILE *file,const char *format,...);
extern int sscanf(const char *string,const char *format,...);

extern int vscanf(const char *format,va_list list);
extern int vfscanf(FILE *file,const char *format,va_list list);
extern int vsscanf(const char *string,const char *format,va_list list);

inline int puts(const char *buf)
{
   return fputs(buf,__stdout); /*Puts to stdout.*/
}

inline char *gets(char *buf)
{
   return fgets(buf,~0,__stdin); /*Gets from stdin.*/
}

