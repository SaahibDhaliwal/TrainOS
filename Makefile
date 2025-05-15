XDIR:=/u/cs452/public/xdev
ARCH=cortex-a72
TRIPLE=aarch64-none-elf
XBINDIR:=$(XDIR)/bin
CC:=$(XBINDIR)/$(TRIPLE)-gcc
OBJCOPY:=$(XBINDIR)/$(TRIPLE)-objcopy
OBJDUMP:=$(XBINDIR)/$(TRIPLE)-objdump

MMU?=on
OPT?=on

# COMPILE OPTIONS
ifeq ($(MMU),on)
MMUFLAGS:=-DMMU
else
MMUFLAGS:=-mstrict-align -mgeneral-regs-only
endif
ifeq ($(OPT),on)
OPTFLAGS=-O3
endif
WARNINGS:=-Wall -Wextra -Wpedantic -Wno-unused-const-variable -Wno-stringop-overflow
CFLAGS:=-g -pipe -static -ffreestanding -mcpu=$(ARCH) -march=armv8-a $(MMUFLAGS) $(OPTFLAGS) $(WARNINGS)

# -Wl,option tells gcc to pass 'option' to the linker with commas replaced by spaces
# doing this rather than calling the linker directly simplifies the compilation procedure
LDFLAGS:=-Wl,-nmagic -Wl,-Tlinker.ld -Wl,--no-warn-rwx-segments -nostartfiles

# Source files and include dirs
SOURCES := $(wildcard *.S) $(wildcard *.c)
# Create .o and .d files for every .c and .S (hand-written assembly) file
OBJECTS := $(patsubst %.S, %.o, $(patsubst %.c, %.o, $(SOURCES)))
DEPENDS := $(patsubst %.S, %.d, $(patsubst %.c, %.d, $(SOURCES)))

# The first rule is the default, ie. "make", "make all" and "make iotest.img" mean the same
all: iotest.img

clean:
	rm -f $(OBJECTS) $(DEPENDS) iotest.elf iotest.img

iotest.img: iotest.elf
	$(OBJCOPY) -S -O binary $< $@

iotest.elf: $(OBJECTS) linker.ld
	$(CC) $(CFLAGS) $(filter-out %.ld, $^) -o $@ $(LDFLAGS)
ifneq ($(MMU),on)
	@$(OBJDUMP) -d $@ | grep -Fq q0 && printf "\n***** WARNING: SIMD DETECTED! *****\n\n" || true
endif

%.o: %.S Makefile
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

%.o: %.c Makefile
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPENDS)
