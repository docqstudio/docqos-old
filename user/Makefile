ROOT=..
include $(ROOT)/Makefile.config

first:dir_lib dir_init dir_date dir_echo dir_ls

dir_init:
	cd init && $(MAKE) -f Makefile
dir_lib:
	cd lib && $(MAKE) -f Makefile
dir_date:
	cd date && $(MAKE) -f Makefile
dir_echo:
	cd echo && $(MAKE) -f Makefile
dir_ls:
	cd ls && $(MAKE) -f Makefile

clean:dir_init_clean dir_lib_clean dir_date_clean dir_echo_clean \
      dir_ls_clean

dir_init_clean:
	cd init && $(MAKE) -f Makefile clean
dir_lib_clean:
	cd lib && $(MAKE) -f Makefile clean
dir_date_clean:
	cd date && $(MAKE) -f Makefile clean
dir_echo_clean:
	cd echo && $(MAKE) -f Makefile clean
dir_ls_clean:
	cd ls && $(MAKE) -f Makefile clean
