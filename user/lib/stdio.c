#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>

static FILE files[] = 
   {
      [0] = 0,
      [1] = 1,
      [2] = 2,
      [3 ... 19] = ~0 /*Unused.*/
   };

FILE * const __stdin = &files[0]; /*The first file.*/
FILE * const __stdout = &files[1]; /*The second file.*/
FILE * const __stderr = &files[3]; /*The third file.*/

FILE *fopen(const char *path,const char *mode)
{
   int i;
   int fd = open(path);
   if(fd < 0)
      return 0;
   for(i = 0;i < sizeof(files) / sizeof(files[0]);++i)
      if(files[i] == ~0)
         goto found;
   close(fd);
   errno = -EBUSY; /*No free file that can used.*/
   return 0;
found:
   files[i] = fd;
   return &files[i];
}

int fclose(FILE *file)
{
   close(*file); /*Close the file.*/
   *file = 0;
   return 0;
}

char *fgets(char *buf,unsigned long size,FILE *file)
{
   int i;
   int fd = *file;
   unsigned long position = lseek(fd,0,SEEK_CUR);
   signed long retval;
   if(size == 0)
      return 0;
   buf[0] = '\0';
   retval = read(fd,buf,size - 1); /*First read.*/
   if(retval <= 0)
      return 0;

   for(i = 0;i < retval;++i)
      if(buf[i] == '\n') /*Then find the '\n'.*/
         break;
   if(i < size - 1)
      buf[++i] = '\0'; /*Set end.*/
   else
      buf[i] = '\0';
   lseek(fd,position + i,SEEK_SET); /*Set position.*/
   return buf;
}

int fputs(const char *buf,FILE *file)
{
   int fd = *file;
   int size = 0;
   while(buf[size] != '\0')
      ++size;
   if(size == 0)
      return 0;
   return write(fd,buf,size); /*Write to it.*/
}

int printf(const char *format,...)
{ /*Call the vprintf.*/
   va_list list;
   int retval;
   va_start(list,format);
   retval = vprintf(format,list);
   va_end(list);
   return retval;
}

int scanf(const char *format,...)
{ /*Call the vscanf.*/
   va_list list;
   int retval;
   va_start(list,format);
   retval = vscanf(format,list);
   va_end(list);
   return retval;
}

int fprintf(FILE *file,const char *format,...)
{ /*Call the vfprintf.*/
   va_list list;
   int retval;
   va_start(list,format);
   retval = vfprintf(file,format,list);
   va_end(list);
   return retval;
}

int fscanf(FILE *file,const char *format,...)
{ /*Call the vfscanf.*/
   va_list list;
   int retval;
   va_start(list,format);
   retval = vfscanf(file,format,list);
   va_end(list);
   return retval;
}

int sprintf(char *string,const char *format,...)
{ /*Call the vsprintf.*/
   va_list list;
   int retval;
   va_start(list,format);
   retval = vsprintf(string,format,list);
   va_end(list);
   return retval;
}

int sscanf(const char *string,const char *format,...)
{ /*Call the vssanf.*/
   va_list list;
   int retval;
   va_start(list,format);
   retval = vsscanf(string,format,list);
   va_end(list);
   return retval;
}

int vprintf(const char *format,va_list list)
{ /*Write to the stdout.*/
   return vfprintf(stdout,format,list);
}

int vscanf(const char *format,va_list list)
{ /*Read from the stdin.*/
   return vfscanf(stdin,format,list);
}

int vfprintf(FILE *file,const char *format,va_list list)
{
   char string[128];
   int retval = vsprintf(string,format,list); /*Get the string.*/
   if(retval <= 0)
      return retval;
   return fputs(string,file); /*Write.*/
}

int vfscanf(FILE *file,const char *format,va_list list)
{
   char string[128];
   int retval = !!fgets(string,128,file);
   if(!retval) /*Failed,return 0.*/
      return 0;
   return vsscanf(string,format,list);
}

int vsprintf(char *string,const char *format,va_list list)
{
   char *__string = string;
   char c;
   char __tmp[128];
   char *tmp = __tmp;

   while((c = *format++))
   {
      if(c != '%')
      {
         *string++ = c;
         continue;
      }
      c = *format++;
      switch(c)
      {
      case 'd':
        {
           int i = va_arg(list,int);
           char s = (i < 0);
           i = (i < 0) ? -i : i; /*i = abs(i);*/
           tmp = __tmp + 127;
           *tmp-- = '\0'; /*First set end.*/
           do{
              *tmp-- = '0' + i % 10;
              i /= 10; /*Write the char.*/
           }while(i > 0);
           if(s)
              *tmp = '-';
           else
              ++tmp;
           while(*tmp)
              *string++ = *tmp++;
        }
        break;
      case 's':
         {
           const char *s = va_arg(list,const char *);
           while(*s) /*Just copy to string.*/
              *string++ = *s++;
        }
        break;
     default:
        break;
      }
   }
   *string = '\0';
   return string - __string;
}

int vsscanf(const char *string,const char *format,va_list list)
{
   char c;
   int retval = 0;

   for(;;)
   {
      while(*format == ' ' || *format == '\n')
         ++format; /*Skip the ' ' or the '\n' of the format.*/
      while(*string == ' ' || *string == '\n')
         ++string; /*Skip the ' ' or the '\n' of the string.*/
      if(*format == '\0')
         break; /*No more,just return.*/
      c = *format++;
      if(c != '%')
      {
         if(*string++ != c)
            return retval; /*Failed!*/
         else
            continue;
      }
      c = *format++;
      switch(c)
      {
      case 'd':
         {
            int *p = va_arg(list,int *);
            const char *start = string;
            const char *end = start;
            int j = 1;
            while('0' <= *end && '9' >= *end)
               ++end; /*How many numbers are there?*/
            if(start == end)
               return retval; /*No numbers,failed.*/
            *p = 0;
            string = (void *)end;
            --end;
            while(end >= start)
               *p += (*end-- - '0') * ((j *= 10) / 10);
            ++retval;
         }
         break;
      case 's':
         {
            char *s = va_arg(list,char *);
            while(*string != '\n' && *string != '\0' && *string != ' ')
               *s++ = *string++; /*Copy until '\n',end or ' '.*/
         }
         break;
      default:
         break;
      }
   }
   return retval; /*Success!!*/
}
