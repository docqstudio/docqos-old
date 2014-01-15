ROOT=..
include $(ROOT)/Makefile.config

first:dir_lib dir_init dir_hello dir_echo

dir_init:
	cd init && $(MAKE) -f Makefile
dir_lib:
	cd lib && $(MAKE) -f Makefile
dir_hello:
	cd hello && $(MAKE) -f Makefile
dir_echo:
	cd echo && $(MAKE) -f Makefile

clean:dir_init_clean dir_lib_clean dir_hello_clean dir_echo_clean

dir_init_clean:
	cd init && $(MAKE) -f Makefile clean
dir_lib_clean:
	cd lib && $(MAKE) -f Makefile clean
dir_hello_clean:
	cd hello && $(MAKE) -f Makefile clean
dir_echo_clean:
	cd echo && $(MAKE) -f Makefile clean
