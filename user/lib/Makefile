ROOT=../..
include $(ROOT)/Makefile.config
#LIB=libcrt.a
OBJS=.obj/string.o .obj/unistd.o .obj/start.o .obj/stdio.o

first:cachedir $(OBJS)
cachedir:
	mkdir -p .obj
.obj/start.o:start.S
	$(AS) $(ASFLAGS_USER) -o $@ $<
.obj/unistd.o:unistd.c ../include/errno.h ../include/unistd.h
	$(CC) $(CFLAGS_USER) -o $@ $<
.obj/string.o:string.c ../include/string.h
	$(CC) $(CFLAGS_USER) -o $@ $<
.obj/stdio.o:stdio.c ../include/stdio.h ../include/unistd.h ../include/errno.h
	$(CC) $(CFLAGS_USER) -o $@ $<

clean:
	$(RM) .obj

#$(LIB):$(OBJS)
#	$(AR) -r $(LIB) $(OBJS)
