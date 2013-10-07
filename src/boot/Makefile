BIN=../../bin

include ../../Makefile.config

OBJS=.obj/boot.o .obj/loader.o
TARGERS=$(BIN)/boot.bin #$(BIN)/loader.bin

first:cachedir $(TARGERS)

cachedir:
	if [ ! -d .obj ]; then \
	mkdir .obj; \
	fi

.obj/boot.o:boot.s
	$(AS) $(ASFLAGS) -o $@ $<

#.obj/loader.o:loader.s

$(BIN)/boot.bin:.obj/boot.o
	$(LD) $(LDFLAGS_BOOT) -o $@ $<

#$(BOOT)/loader.bin:.obj/loader.o

clean:
	$(RM) .obj
