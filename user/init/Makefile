ROOT=../..
include $(ROOT)/Makefile.config

OBJS=.obj/cinit.o $(LDCRT_USER)
TARGET=$(ROOT)/bin/sbin/init

first:cachedir $(TARGET)

cachedir:
	mkdir -p .obj

.obj/cinit.o:init.c ../include/unistd.h
	$(CC) $(CFLAGS_USER) -o $@ $<

$(TARGET):$(OBJS)
	mkdir -p $(ROOT)/sbin
	$(LD) $(LDFLAGS_USER) -o $@ $(OBJS)

clean:
	$(RM) .obj
