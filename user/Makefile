ROOT=..
include $(ROOT)/Makefile.config

first:dir_init

dir_init:
	cd init && $(MAKE) -f Makefile

clean:dir_init_clean

dir_init_clean:
	cd init && $(MAKE) -f Makefile clean
