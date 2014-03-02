ROOT=../..
include $(ROOT)/Makefile.config
TARGET=$(ROOT)/bin/bin/complex
OBJS=.obj/complex.o $(LDCRT_USER)

first:cachedir $(TARGET)

cachedir:
	mkdir -p .obj
.obj/complex.o:complex.c ../include/unistd.h
	$(CC) $(CFLAGS_USER) -o $@ $<
$(TARGET):$(OBJS) 
	$(LD) $(LDFLAGS_USER) -o $(TARGET) $(OBJS)

clean:
	$(RM) .obj
