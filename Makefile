ROOT=.
include Makefile.config

first:dir_src dir_user $(ISO)

dir_src:
	cd src && ./configure && $(MAKE) -f Makefile
dir_user:
	cd user && $(MAKE) -f Makefile

$(ISO):bin bin/* bin/*/*
	$(MKRESCUE) -o $(ISO) $(ROOT)/bin

qrun:first
	$(QEMU) $(QEMUFLAGS)

brun:first
	$(BOCHS) $(BOCHSFLAGS)

clean:dir_src_clean dir_user_clean

clean_all:clean
	$(RM) src/Makefile bin/bin
	$(RM) parport.out os.iso map.map bochsout.txt $(KERNEL_ELF) user/lib/lib*.a
	$(RM) `ls -al bin | sed '1d' | awk '{if(substr($$0,0,1)!="d" && substr($$0,10,1)=="x")printf "bin/"$$9" "}END{printf "\n"}'`
	$(RM) `ls -al bin/sbin | sed '1d' | awk '{if(substr($$0,0,1)!="d" && substr($$0,10,1)=="x")printf "bin/sbin/"$$9" "}END{printf "\n"}'`

dir_src_clean:
	cd src && $(MAKE) -f Makefile clean
dir_user_clean:
	cd user && $(MAKE) -f Makefile clean

all:clean first

gdb:first
	$(QEMU) $(QEMUFLAGS_DEBUG) &
	$(GDB) $(GDBFLAGS)
