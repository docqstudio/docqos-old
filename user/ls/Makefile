ROOT=../..
include $(ROOT)/Makefile.config
TARGET=$(ROOT)/bin/bin/ls
OBJS=.obj/ls.o $(LDCRT_USER)

first:cachedir $(TARGET)

cachedir:
	mkdir -p .obj
.obj/ls.o:ls.c ../include/unistd.h $(LDCRT_USER)
	$(CC) $(CFLAGS_USER) -o $@ $<
$(TARGET):$(OBJS)
	$(LD) $(LDFLAGS_USER) -o $(TARGET) $(OBJS)

clean:
	$(RM) .obj
