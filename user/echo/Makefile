ROOT=../..
include $(ROOT)/Makefile.config
TARGET=$(ROOT)/bin/bin/echo
OBJS=.obj/echo.o $(LDCRT_USER)

first:cachedir $(TARGET)

cachedir:
	mkdir -p .obj
.obj/echo.o:echo.c ../include/unistd.h
	$(CC) $(CFLAGS_USER) -o $@ $<
$(TARGET):$(OBJS) 
	$(LD) $(LDFLAGS_USER) -o $(TARGET) $(OBJS)

clean:
	$(RM) .obj
