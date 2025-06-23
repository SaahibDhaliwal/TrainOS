XDIR:=/u/cs452/public/xdev
ARCH=cortex-a72
TRIPLE=aarch64-none-elf
XBINDIR:=$(XDIR)/bin
CXX:=$(XBINDIR)/$(TRIPLE)-g++
OBJCOPY:=$(XBINDIR)/$(TRIPLE)-objcopy
OBJDUMP:=$(XBINDIR)/$(TRIPLE)-objdump
OUT_DIR := build
TARGET := kernel4

MMU?=on
OPT?=on
TESTING?=off

# COMPILE OPTIONS
ifeq ($(MMU),on)
MMUFLAGS:=-DMMU
else
MMUFLAGS:=-mstrict-align -mgeneral-regs-only
endif

ifeq ($(OPT),on)
OPTFLAGS=-O3
endif

ifeq ($(TESTING),on)
TESTFLAGS:=-DTESTING
endif

WARNINGS:=-Wall -Wextra -Wpedantic -Werror=return-type -Wno-unused-const-variable -Wno-stringop-overflow
# I had to get rid of freestanding for the map. Says its fine on piazza
CFLAGS:= -Isrc -Isrc/containers -Iinclude/clients -Iinclude/servers -Iinclude/protocols -Iinclude -Itests -g -pipe -static -mcpu=$(ARCH) -march=armv8-a $(MMUFLAGS) $(OPTFLAGS) $(TESTFLAGS) $(WARNINGS)

# -Wl,option tells gcc to pass 'option' to the linker with commas replaced by spaces
# doing this rather than calling the linker directly simplifies the compilation procedure
LDFLAGS:=-Wl,-nmagic -Wl,-Tlinker.ld -Wl,--no-warn-rwx-segments -nostartfiles -lstdc++ -lgcc

# Source files and include dirs
SOURCES := $(shell find . -path ./demofiles -prune -o -type f \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.S' \) -print)

OBJECTS := $(patsubst ./%, $(OUT_DIR)/%, $(SOURCES:.c=.o))
OBJECTS := $(OBJECTS:.cc=.o)
OBJECTS := $(OBJECTS:.cpp=.o)
OBJECTS := $(OBJECTS:.S=.o)
DEPENDS := $(OBJECTS:.o=.d)

# The first rule is the default, ie. "make", "make all" and "make kernel1.img" mean the same
all: clean $(TARGET).img

clean:
	rm -f $(OBJECTS) $(DEPENDS) $(TARGET).elf $(TARGET).img

.PHONY: test
test:
	 $(MAKE) clean all TESTING=on

$(TARGET).img: $(TARGET).elf
	$(OBJCOPY) -S -O binary $< $@

$(TARGET).elf: $(OBJECTS) linker.ld
	$(CXX) $(CFLAGS) $(filter-out %.ld, $^) -o $@ $(LDFLAGS)
ifneq ($(MMU),on)
	@$(OBJDUMP) -d $@ | grep -Fq q0 && printf "\n***** WARNING: SIMD DETECTED! *****\n\n" || true
endif

$(OUT_DIR)/%.o: %.S Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(CFLAGS) -MMD -MP -c $< -o $@

$(OUT_DIR)/%.o: %.c Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(CFLAGS) -MMD -MP -c $< -o $@

$(OUT_DIR)/%.o: %.cpp Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(CFLAGS) -MMD -MP -c $< -o $@

$(OUT_DIR)/%.o: %.cc Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPENDS)
