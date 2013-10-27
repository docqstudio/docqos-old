ROOT=../..
BIN=$(ROOT)/bin
include ../../Makefile.config
ASFLAGS+=-I ./include

OBJS=.obj/boot.o .obj/loader.o
TARGETS=$(BIN)/boot.bin $(BIN)/loader.bin

first:cachedir $(TARGETS)

cachedir:
	if [ ! -d .obj ]; then \
	mkdir .obj; \
	fi

.obj/boot.o:boot.S
	$(AS) $(ASFLAGS) -o $@ $<

.obj/loader.o:loader.S
	$(AS) $(ASFLAGS) -o $@ $<

$(BIN)/boot.bin:.obj/boot.o include/boot.inc.s
	$(LD) $(LDFLAGS_BIN) -o $@ $<

$(BIN)/loader.bin:.obj/loader.o include/boot.inc.S include/pmode.inc.S include/lmode.inc.S
	$(LD) $(LDFLAGS_BIN) -o $@ $<
clean:
	$(RM) .obj
