ROOT=..
include ../Makefile.config

TARGET=$(ROOT)/bin/kernel
OBJS=.obj/start.o .obj/cstart.o .obj/vesa.o .obj/string.o .obj/font.o

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
.obj/cstart.o:init/cstart.c ../include/video/vesa.h ../include/lib/string.h ../include/core/const.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/string.o:lib/string.c ../include/core/const.h ../include/lib/string.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/vesa.o:video/vesa.c ../include/video/vesa.h ../include/lib/string.h ../include/core/const.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/font.o:video/font.c ../include/core/const.h
	$(CC) $(CFLAGS) -o $@ $<
$(TARGET):$(OBJS) $(ROOT)/ldscripts/kernel.lds
	$(LD) $(LDFLAGS_ELF) -T$(ROOT)/ldscripts/kernel.lds -o $@ $(OBJS)
	$(TOBINARY) $(TARGET) $(TARGET)
