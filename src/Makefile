ROOT=..
include ../Makefile.config

TARGET=$(ROOT)/bin/kernel
OBJS=.obj/start.o .obj/cstart.o .obj/vesa.o .obj/string.o .obj/font.o \
   .obj/console.o .obj/memory.o .obj/cpuid.o .obj/idt.o .obj/exception.o \
   .obj/cexception.o .obj/pic.o .obj/interrupt.o .obj/cinterrupt.o \
   .obj/acpi.o .obj/localapic.o .obj/ioapic.o

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
.obj/cstart.o:init/cstart.c ../include/video/vesa.h ../include/lib/string.h ../include/core/const.h \
              ../include/memory/memory.h ../include/video/console.h ../include/cpu/cpuid.h \
	      ../include/interrupt/interrupt.h ../include/acpi/acpi.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/string.o:lib/string.c ../include/core/const.h ../include/lib/string.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/vesa.o:video/vesa.c ../include/video/vesa.h ../include/lib/string.h ../include/core/const.h 
	$(CC) $(CFLAGS) -o $@ $<
.obj/font.o:video/font.c ../include/core/const.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/console.o:video/console.c ../include/core/const.h ../include/video/vesa.h ../include/lib/stdarg.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/memory.o:memory/memory.c ../include/core/const.h ../include/memory/memory.h ../include/video/console.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/cpuid.o:cpu/cpuid.c ../include/core/const.h ../include/video/console.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/idt.o:interrupt/idt.c ../include/core/const.h ../include/video/console.h ../include/interrupt/idt.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/exception.o:interrupt/exception.S
	$(AS) $(ASFLAGS) -o $@ $<
.obj/cexception.o:interrupt/exception.c ../include/core/const.h ../include/video/console.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/pic.o:interrupt/pic.c ../include/core/const.h ../include/cpu/io.h ../include/interrupt/pic.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/interrupt.o:interrupt/interrupt.S
	$(AS) $(ASFLAGS) -o $@ $<
.obj/cinterrupt.o:interrupt/interrupt.c ../include/video/console.h ../include/core/const.h \
                  ../include/interrupt/pic.h ../include/interrupt/idt.h ../include/cpu/io.h \
		  ../include/interrupt/localapic.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/acpi.o:acpi/acpi.c ../include/core/const.h ../include/video/console.h ../include/acpi/acpi.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/localapic.o:interrupt/localapic.c ../include/core/const.h ../include/acpi/acpi.h \
                 ../include/cpu/cpuid.h ../include/video/console.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/ioapic.o:interrupt/ioapic.c ../include/core/const.h ../include/acpi/acpi.h ../include/video/console.h \
              ../include/interrupt/ioapic.h ../include/interrupt/localapic.h
	$(CC) $(CFLAGS) -o $@ $<
$(TARGET):$(OBJS) $(ROOT)/ldscripts/kernel.lds
	$(LD) $(LDFLAGS_ELF) -T$(ROOT)/ldscripts/kernel.lds -o $@ $(OBJS)
	$(TOBINARY) $(TARGET) $(TARGET)
