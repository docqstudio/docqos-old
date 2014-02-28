#include <core/const.h>
#include <task/elf64.h>
#include <filesystem/virtual.h>
#include <memory/paging.h>
#include <interrupt/interrupt.h>
#include <cpu/gdt.h>
#include <cpu/io.h>
#include <lib/string.h>

/*See also http://downloads.openwatcom.org/ftp/devel/docs/elf-64-gen.pdf.*/

typedef struct ELF64Header{
   u8 ident[16];
   u16 type;
   u16 machine;
   u32 version;
   u64 entry;
   u64 phoff;
   u64 shoff;
   u32 flags;
   u16 ehsize;
   u16 phentsize;
   u16 phnum;
   u16 shentsize;
   u16 shnum;
   u16 shstrndx;
} ELF64Header;

typedef struct ELF64ProgramHeader{
   u32 type;
   u32 flags;
   u64 offset;
   u64 vaddr;
   u64 pddr;
   u64 filesz;
   u64 memsz;
   u64 align;
} ELF64ProgramHeader;

int elf64Execve(VFSFile *file,u8 *arguments,u64 pos,u64 end,IRQRegisters *regs)
{
   ELF64Header header;
   int retval;
   if(lseekFile(file,0) < 0)
      return -ENOEXEC;
   if((retval = readFile(file,&header,sizeof(header))) <= 0) /*Read header.*/
      return retval == -EIO ? -EIO : -ENOEXEC;

   if(header.ident[0] != 0x7f ||
      header.ident[1] != 'E' ||
      header.ident[2] != 'L' ||
      header.ident[3] != 'F') /*Is it an elf file?*/
      return -ENOEXEC;
   if(header.ident[4] != 2) /*Is it 64-bit?*/
      return -ENOEXEC;
   if(header.phentsize != sizeof(ELF64ProgramHeader))
      return -ENOEXEC; /*Is the program headers' size right?*/

   if(lseekFile(file,header.phoff) < 0) /*Seek file to the program headers' position.*/
      return -ENOEXEC;
   ELF64ProgramHeader phdrs[header.phnum];
   if((retval = readFile(file,phdrs,sizeof(phdrs))) <= 0) /*Read them!*/
      return retval == -EIO ? -EIO : -ENOEXEC;

   for(int i = 0;i < sizeof(phdrs) / sizeof(phdrs[0]);++i)
   {
      if(phdrs[i].type != 1)
         continue;
      if(phdrs[i].memsz == 0)
         continue;
      if(phdrs[i].filesz == 0) /*The data is not in file!!!!*/
         retval = doMMap(0,0,phdrs[i].vaddr,phdrs[i].memsz);
      else
         retval = doMMap(file,phdrs[i].offset,phdrs[i].vaddr,phdrs[i].memsz);
      if(retval)
         return retval;
   }
   if((retval = doMMap(0,0,0xffffe000,0x2000)))
      return retval;
       /*Map the user stack,from 0xffffe000 to 0xffffffff.*/
       /*(4GB - 8K) ~ 4GB.*/
   pointer stackTop = 0xfffffffful;
   regs->rcx = regs->rbx = 0;
   if(!arguments) /*Are there arguments?*/
      goto out;
   stackTop -= end; /*Put the arguments to the user stack.*/
   for(int i = pos;i < end - sizeof(void *);i += sizeof(void *))
      *(pointer *)(&arguments[i]) += stackTop;
   memcpy((void *)stackTop,(const void *)arguments,end);
   regs->rcx = (end - pos) / sizeof(void *) - 1;
   regs->rbx = stackTop + pos;
    /*User: %rcx => argument count (argc),%rbx => arguments (argv).*/
out:
   regs->rip = header.entry; /*Set rip,cs,ss,rsp and rflags.*/
   regs->cs = SELECTOR_USER_CODE;
   regs->ss = SELECTOR_USER_DATA;
   regs->rsp = (u64)stackTop;
   regs->rflags = storeInterrupt(); /*Interrupts should be started.*/
   return 0;
}
