ROOT=../..
BIN=$(ROOT)/bin
include ../../Makefile.config
ASFLAGS=-I ./include

OBJS=.obj/boot.o .obj/loader.o
TARGETS=$(BIN)/boot.bin $(BIN)/loader.bin

first:cachedir $(TARGETS)

cachedir:
	if [ ! -d .obj ]; then \
	mkdir .obj; \
	fi

.obj/boot.o:boot.S include/boot.inc.S
	$(AS) $(ASFLAGS) -o $@ $<

.obj/loader.o:loader.S include/boot.inc.S include/pmode.inc.S include/lmode.inc.S
	$(AS) $(ASFLAGS) -o $@ $<

$(BIN)/boot.bin:.obj/boot.o 
	$(LD) $(LDFLAGS_BIN) -o $@ $<

$(BIN)/loader.bin:.obj/loader.o
	$(LD) $(LDFLAGS_BIN) -o $@ $<
clean:
	$(RM) .obj
