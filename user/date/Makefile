ROOT=../..
include $(ROOT)/Makefile.config
TARGET=$(ROOT)/bin/bin/date
OBJS=.obj/date.o $(LDCRT_USER)

first:cachedir $(TARGET)

cachedir:
	mkdir -p .obj
.obj/date.o:date.c ../include/unistd.h $(LDCRT_USER)
	$(CC) $(CFLAGS_USER) -o $@ $<
$(TARGET):$(OBJS)
	$(LD) $(LDFLAGS_USER) -o $(TARGET) $(OBJS)

clean:
	$(RM) .obj
