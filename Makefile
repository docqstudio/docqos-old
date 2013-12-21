ROOT=.
include Makefile.config

first:dir_src $(ISO)

dir_src:
	cd src && $(MAKE) -f Makefile
	
$(ISO):bin
	$(MKISO) $(MKISOFLAGS) -o os.iso bin

qrun:first
	$(QEMU) $(QEMUFLAGS)

brun:first
	$(BOCHS) $(BOCHSFLAGS)

clean:dir_src_clean

clean_all:clean
	$(RM) bin/*.bin bin/kernel parport.out os.iso map.map bochsout.txt 

dir_src_clean:
	cd src && $(MAKE) -f Makefile clean

all: clean first
