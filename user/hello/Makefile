ROOT=../..
include $(ROOT)/Makefile.config
TARGET=$(ROOT)/bin/hello
OBJS=.obj/hello.o

first:cachedir $(TARGET)

cachedir:
	mkdir -p .obj
.obj/hello.o:hello.c ../include/unistd.h
	$(CC) $(CFLAGS_USER) -o $@ $<
$(TARGET):$(OBJS)
	$(LD) $(LDFLAGS_USER) -o $(TARGET) $(OBJS)

clean:
	$(RM) .obj
