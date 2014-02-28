ROOT=../..
include $(ROOT)/Makefile.config
TARGET=$(ROOT)/bin/bin/cat
OBJS=.obj/cat.o $(LDCRT_USER)

first:cachedir $(TARGET)

cachedir:
	mkdir -p .obj
.obj/cat.o:cat.c ../include/unistd.h
	$(CC) $(CFLAGS_USER) -o $@ $<
$(TARGET):$(OBJS)
	$(LD) $(LDFLAGS_USER) -o $(TARGET) $(OBJS)

clean:
	$(RM) .obj
