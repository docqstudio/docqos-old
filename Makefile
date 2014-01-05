ROOT=.
include Makefile.config

first:dir_src dir_user $(ISO)

dir_src:
	cd src && $(MAKE) -f Makefile
dir_user:
	cd user && $(MAKE) -f Makefile

$(ISO):bin
	$(MKISO) $(MKISOFLAGS) -o os.iso bin

qrun:first
	$(QEMU) $(QEMUFLAGS)

brun:first
	$(BOCHS) $(BOCHSFLAGS)

clean:dir_src_clean dir_user_clean

clean_all:clean
	$(RM) bin/*.bin bin/kernel parport.out os.iso map.map bochsout.txt 
	$(RM) kernel.elf bin/sbin/init

dir_src_clean:
	cd src && $(MAKE) -f Makefile clean
dir_user_clean:
	cd user && $(MAKE) -f Makefile clean

all:clean first

gdb:first
	$(QEMU) $(QEMUFLAGS_DEBUG) &
	$(GDB) $(GDBFLAGS)
