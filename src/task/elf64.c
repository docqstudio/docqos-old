#include <core/const.h>
#include <task/elf64.h>
#include <filesystem/virtual.h>
#include <memory/paging.h>
#include <interrupt/interrupt.h>
#include <cpu/gdt.h>
#include <cpu/io.h>

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

int elf64Execve(VFSFile *file,const char *argv[],const char *envp[],IRQRegisters *regs)
{
   ELF64Header header;
   if(lseekFile(file,0) < 0)
      return -1;
   if(readFile(file,&header,sizeof(header)) <= 0) /*Read header.*/
      return -1;

   if(header.ident[0] != 0x7f ||
      header.ident[1] != 'E' ||
      header.ident[2] != 'L' ||
      header.ident[3] != 'F') /*Is it an elf file?*/
      return -1;
   if(header.ident[4] != 2) /*Is it 64-bit?*/
      return -1;
   if(header.phentsize < sizeof(ELF64ProgramHeader) ||
      header.phentsize % sizeof(ELF64ProgramHeader) != 0)
      return -1; /*Is the program headers' size right?*/

   if(lseekFile(file,header.phoff) < 0) /*Seek file to the program headers' position.*/
      return -1;
   ELF64ProgramHeader phdrs[header.phentsize / sizeof(ELF64ProgramHeader)];
   if(readFile(file,phdrs,sizeof(phdrs)) <= 0) /*Read them!*/
      return -1;

   for(int i = 0;i < sizeof(phdrs) / sizeof(phdrs[0]);++i)
      doMMap(file,phdrs[i].offset,phdrs[i].vaddr,phdrs[i].memsz);
   doMMap(0,0,0xffffe000,0x2000);
       /*Map the user stack,from 0xffffe000 to 0xffffffff.*/
       /*(4GB - 8K) ~ 4GB.*/

   regs->rip = header.entry; /*Set rip,cs,ss,rsp and rflags.*/
   regs->cs = SELECTOR_USER_CODE;
   regs->ss = SELECTOR_USER_DATA;
   regs->rsp = 0xffffffff;
   regs->rflags = storeInterrupt(); /*Interrupts should be started.*/
   return 0;
}
