ROOT=../..
include $(ROOT)/Makefile.config

OBJS=.obj/init.o .obj/cinit.o
TARGET=$(ROOT)/bin/sbin/init

first:cachedir $(TARGET)

cachedir:
	mkdir -p .obj

.obj/init.o:init.S
	$(AS) $(ASFLAGS) -o $@ $<
.obj/cinit.o:init.c
	$(CC) $(CFLAGS) -o $@ $<

$(TARGET):$(OBJS)
	mkdir -p $(ROOT)/sbin/init
	$(LD) -s -e _start -o $@ $(OBJS)

clean:
	$(RM) .obj
