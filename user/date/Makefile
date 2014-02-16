ROOT=../..
include $(ROOT)/Makefile.config
TARGET=$(ROOT)/bin/bin/date
OBJS=.obj/date.o

first:cachedir $(TARGET)

cachedir:
	mkdir -p .obj
.obj/date.o:date.c ../include/unistd.h
	$(CC) $(CFLAGS_USER) -o $@ $<
$(TARGET):$(OBJS)
	$(LD) $(LDFLAGS_USER) -o $(TARGET) $(OBJS)

clean:
	$(RM) .obj
