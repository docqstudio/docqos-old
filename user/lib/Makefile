ROOT=../..
include $(ROOT)/Makefile.config
LIB=libcrt.a
OBJS=.obj/unistd.o .obj/start.o

first:cachedir $(LIB)
cachedir:
	mkdir -p .obj
.obj/start.o:start.S
	$(AS) $(ASFLAGS_USER) -o $@ $<
.obj/unistd.o:unistd.c
	$(CC) $(CFLAGS_USER) -o $@ $<

clean:
	$(RM) .obj

$(LIB):$(OBJS)
	$(AR) -r $(LIB) $(OBJS)
