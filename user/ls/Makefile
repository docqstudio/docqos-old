ROOT=../..
include $(ROOT)/Makefile.config
TARGET=$(ROOT)/bin/bin/ls
OBJS=.obj/ls.o

first:cachedir $(TARGET)

cachedir:
	mkdir -p .obj
.obj/ls.o:ls.c ../include/unistd.h
	$(CC) $(CFLAGS_USER) -o $@ $<
$(TARGET):$(OBJS)
	$(LD) $(LDFLAGS_USER) -o $(TARGET) $(OBJS)

clean:
	$(RM) .obj
