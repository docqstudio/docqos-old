ROOT=..
include ../Makefile.config

first:dir_boot

dir_boot: boot
	cd boot && $(MAKE) -f Makefile

clean: dir_boot_clean

dir_boot_clean:
	cd boot && $(MAKE) -f Makefile clean
