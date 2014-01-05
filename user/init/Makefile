ROOT=../..
include $(ROOT)/Makefile.config

OBJS=.obj/init.o
TARGET=$(ROOT)/bin/sbin/init

first:cachedir $(TARGET)

cachedir:
	mkdir -p .obj

.obj/init.o:init.S
	$(AS) $(ASFLAGS) -o $@ $<

$(TARGET):$(OBJS)
	$(LD) -s -e _start -o $@ $(OBJS)

clean:
	$(RM) .obj
