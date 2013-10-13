ROOT=../..
BIN=$(ROOT)/bin
include ../../Makefile.config
ASFLAGS+=-I ./include

OBJS=.obj/boot.o .obj/loader.o
TARGERS=$(BIN)/boot.bin $(BIN)/loader.bin

first:cachedir $(TARGERS)

cachedir:
	if [ ! -d .obj ]; then \
	mkdir .obj; \
	fi

.obj/boot.o:boot.s
	$(AS) $(ASFLAGS) -o $@ $<

.obj/loader.o:loader.s
	$(AS) $(ASFLAGS) -o $@ $<

$(BIN)/boot.bin:.obj/boot.o
	$(LD) $(LDFLAGS_BIN) -o $@ $<

$(BIN)/loader.bin:.obj/loader.o
	$(LD) $(LDFLAGS_BIN) -o $@ $<
clean:
	$(RM) .obj
