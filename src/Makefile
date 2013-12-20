ROOT=..
include ../Makefile.config

TARGET=$(ROOT)/bin/kernel
OBJS=.obj/start.o .obj/cstart.o .obj/vesa.o .obj/string.o .obj/font.o \
   .obj/console.o .obj/memory.o .obj/cpuid.o .obj/idt.o .obj/exception.o \
   .obj/cexception.o .obj/pic.o .obj/interrupt.o .obj/cinterrupt.o \
   .obj/acpi.o .obj/localapic.o .obj/ioapic.o .obj/buddy.o .obj/slab.o \
   .obj/kmalloc.o .obj/paging.o .obj/hpet.o .obj/time.o .obj/localtime.o \
   .obj/pit.o .obj/gdt.o .obj/task.o .obj/fork.o .obj/driver.o \
   .obj/pci.o .obj/ide.o .obj/semaphore.o

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
	      ../include/interrupt/interrupt.h ../include/acpi/acpi.h ../include/memory/paging.h \
	      ../include/time/time.h ../include/time/localtime.h ../include/cpu/gdt.h \
	      ../include/task/task.h ../include/core/list.h ../include/cpu/spinlock.h \
              ../include/cpu/spinlock_types.h ../include/driver/driver.h ../include/driver/pci.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/string.o:lib/string.c ../include/core/const.h ../include/lib/string.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/vesa.o:video/vesa.c ../include/video/vesa.h ../include/lib/string.h ../include/core/const.h  \
            ../include/memory/paging.h ../include/cpu/spinlock.h ../include/cpu/io.h ../include/task/task.h \
            ../include/cpu/spinlock_types.h ../include/task/semaphore.h ../include/task/waitqueue.h \
	    ../include/cpu/atomic.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/font.o:video/font.c ../include/core/const.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/console.o:video/console.c ../include/core/const.h ../include/video/vesa.h ../include/lib/stdarg.h \
               ../include/video/console.h ../include/lib/string.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/memory.o:memory/memory.c ../include/core/const.h ../include/memory/memory.h ../include/video/console.h \
              ../include/memory/buddy.h ../include/core/list.h ../include/memory/paging.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/cpuid.o:cpu/cpuid.c ../include/core/const.h ../include/video/console.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/idt.o:interrupt/idt.c ../include/core/const.h ../include/video/console.h ../include/interrupt/idt.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/exception.o:interrupt/exception.S
	$(AS) $(ASFLAGS) -o $@ $<
.obj/cexception.o:interrupt/exception.c ../include/core/const.h ../include/video/console.h \
                  ../include/lib/string.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/pic.o:interrupt/pic.c ../include/core/const.h ../include/cpu/io.h ../include/interrupt/pic.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/interrupt.o:interrupt/interrupt.S
	$(AS) $(ASFLAGS) -o $@ $<
.obj/cinterrupt.o:interrupt/interrupt.c ../include/video/console.h ../include/core/const.h \
                  ../include/interrupt/pic.h ../include/interrupt/idt.h ../include/cpu/io.h \
		  ../include/interrupt/localapic.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/acpi.o:acpi/acpi.c ../include/core/const.h ../include/video/console.h ../include/acpi/acpi.h \
            ../include/memory/paging.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/localapic.o:interrupt/localapic.c ../include/core/const.h ../include/acpi/acpi.h \
                 ../include/cpu/cpuid.h ../include/video/console.h ../include/interrupt/idt.h \
		 ../include/driver/driver.h ../include/core/list.h ../include/cpu/spinlock_types.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/ioapic.o:interrupt/ioapic.c ../include/core/const.h ../include/acpi/acpi.h ../include/video/console.h \
              ../include/interrupt/ioapic.h ../include/interrupt/localapic.h ../include/interrupt/idt.h \
	      ../include/driver/driver.h ../include/core/list.h ../include/cpu/spinlock_types.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/buddy.o:memory/buddy.c ../include/core/const.h ../include/memory/buddy.h ../include/video/console.h \
             ../include/core/list.h ../include/memory/memory.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/slab.o:memory/slab.c ../include/core/const.h ../include/memory/slab.h ../include/video/console.h \
            ../include/core/list.h ../include/memory/buddy.h ../include/memory/paging.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/kmalloc.o:memory/kmalloc.c ../include/core/const.h ../include/core/list.h ../include/memory/buddy.h \
               ../include/memory/slab.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/paging.o:memory/paging.c ../include/core/const.h ../include/memory/paging.h ../include/memory/memory.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/hpet.o:time/hpet.c ../include/core/const.h ../include/memory/paging.h ../include/time/hpet.h \
            ../include/video/console.h ../include/acpi/acpi.h ../include/lib/string.h ../include/driver/driver.h \
	    ../include/core/list.h ../include/cpu/spinlock_types.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/time.o:time/time.c ../include/core/const.h ../include/time/time.h ../include/time/hpet.h \
            ../include/video/console.h ../include/interrupt/interrupt.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/localtime.o:time/localtime.c ../include/core/const.h ../include/interrupt/interrupt.h \
                 ../include/time/localtime.h ../include/video/console.h ../include/interrupt/localapic.h \
		 ../include/cpu/io.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/pit.o:time/pit.c ../include/core/const.h ../include/interrupt/interrupt.h ../include/cpu/io.h \
           ../include/video/console.h ../include/time/pit.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/gdt.o:cpu/gdt.c ../include/core/const.h ../include/cpu/gdt.h ../include/video/console.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/task.o:task/task.c ../include/core/const.h ../include/cpu/io.h ../include/task/task.h \
            ../include/video/console.h ../include/core/list.h ../include/cpu/spinlock.h \
	    ../include/memory/buddy.h ../include/cpu/atomic.h ../include/lib/string.h \
	    ../include/interrupt/interrupt.h ../include/cpu/gdt.h ../include/cpu/spinlock_types.h 
	$(CC) $(CFLAGS) -o $@ $<
.obj/fork.o:task/fork.S
	$(AS) $(ASFLAGS) -o $@ $<
.obj/driver.o:driver/driver.c ../include/core/const.h ../include/driver/driver.h ../include/core/list.h \
              ../include/cpu/spinlock_types.h ../include/cpu/spinlock.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/pci.o:driver/pci.c ../include/driver/driver.h ../include/core/const.h ../include/driver/pci.h \
           ../include/video/console.h ../include/cpu/io.h ../include/memory/kmalloc.h ../include/core/list.h \
	   ../include/cpu/spinlock.h ../include/cpu/spinlock_types.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/ide.o:driver/ide.c ../include/core/const.h ../include/video/console.h ../include/driver/driver.h \
            ../include/core/list.h ../include/cpu/spinlock_types.h ../include/cpu/spinlock.h \
	    ../include/driver/pci.h ../include/cpu/io.h ../include/lib/string.h ../include/interrupt/interrupt.h
	$(CC) $(CFLAGS) -o $@ $<
.obj/semaphore.o: task/semaphore.c ../include/core/const.h ../include/task/semaphore.h \
                  ../include/task/waitqueue.h ../include/task/task.h ../include/core/list.h \
		  ../include/cpu/spinlock_types.h ../include/interrupt/interrupt.h ../include/cpu/spinlock.h \
                  ../include/cpu/atomic.h
	$(CC) $(CFLAGS) -o $@ $<

$(TARGET):$(OBJS) $(ROOT)/ldscripts/kernel.lds
	$(LD) $(LDFLAGS_ELF) -T$(ROOT)/ldscripts/kernel.lds -o $@ $(OBJS)
	$(TOBINARY) $(TARGET) $(TARGET)
