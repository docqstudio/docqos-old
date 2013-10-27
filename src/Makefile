ROOT=..
include ../Makefile.config

TARGET=$(ROOT)/bin/kernel
OBJS=.obj/start.o

first:dir_boot cachedir $(TARGET)

dir_boot: boot
	cd boot && $(MAKE) -f Makefile

clean: dir_boot_clean
	$(RM) .obj

dir_boot_clean:
	cd boot && $(MAKE) -f Makefile clean

cachedir:
	if [ ! -d .obj ]; then \
	mkdir .obj; \
	fi

.obj/start.o:init/start.S
	$(AS) $(ASFLAGS) -o $@ $<
$(TARGET):$(OBJS) $(ROOT)/ldscripts/kernel.lds
	$(LD) $(LDFLAGS_ELF) -T$(ROOT)/ldscripts/kernel.lds -o $@ $(OBJS)
	$(TOBINARY) $(TARGET) $(TARGET)