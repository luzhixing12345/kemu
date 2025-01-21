
ifeq ($(strip $(V)),)
	ifeq ($(findstring s,$(filter-out --%,$(firstword $(MAKEFLAGS)))),)
		E = @echo
	else
		E = @\#
	endif
	Q = @
else
	E = @\#
	Q =
endif
export E Q

VERSION 	:= 0
PATCHLEVEL 	:= 0
SUBLEVEL 	:= 1

# Translate uname -m into ARCH string
ARCH ?= $(shell uname -m | sed -e s/i.86/i386/ -e s/ppc.*/powerpc/ \
	  -e s/armv.*/arm/ -e s/aarch64.*/arm64/ -e s/mips64/mips/ \
	  -e s/riscv64/riscv/ -e s/riscv32/riscv/)

# Default to using host tools
CROSS_COMPILE 	:=
CC          	:= gcc
TARGET      	:= kemu
SRC_PATH    	:= init include/simple-clib virtio disk net
SRC_EXT     	:= c
THIRD_LIB   	:=
INCLUDE_PATH 	:= include arch/$(ARCH)/include

FIND	    := find
CSCOPE	    := cscope
TAGS	    := ctags
INSTALL     := install
CHECK       := check
OBJCOPY		:= objcopy
# ------------------------- #
#          FLAGS            #
# ------------------------- #
CFLAGS = -DCONFIG_VERSION=\"$(VERSION).$(PATCHLEVEL).$(SUBLEVEL)\"
LDFLAGS =
LIBS		= 
LIBS		+= -lpthread
LIBS		+= -lrt
DEFINES     = 
DEFINES		+= -D_FILE_OFFSET_BITS=64
DEFINES		+= -D_GNU_SOURCE
# ------------------------- #

ifneq ($(strip $(INCLUDE_PATH)),)
    CFLAGS += $(foreach dir, $(INCLUDE_PATH), -I$(dir))
endif

# ------------------------- #
#          WARNING          #
# ------------------------- #
WARNING += -Wall
WARNING += -Wunused
WARNING += -Wformat-security
WARNING += -Wshadow
WARNING += -Wpedantic
WARNING += -Wstrict-aliasing
WARNING += -Wuninitialized
WARNING += -Wnull-dereference
WARNING += -Wformat=2
WARNING += -fno-strict-aliasing
WARNING += -Winit-self
WARNING += -Wnested-externs
WARNING += -Wno-system-headers
WARNING += -Wredundant-decls
WARNING += -Wsign-compare
WARNING += -Wundef
WARNING += -Wvolatile-register-var
WARNING += -Wwrite-strings
WARNING += -Wno-format-nonliteral
WARNING += -Wno-pedantic -Wno-discarded-qualifiers
WARNING += -Wno-incompatible-pointer-types-discards-qualifiers
CFLAGS	+= $(WARNING)
# ------------------------- #


ifneq ($(THIRD_LIB),)
CFLAGS 	+= $(shell pkg-config --cflags $(THIRD_LIB))
LDFLAGS += $(shell pkg-config --libs $(THIRD_LIB))
endif

TEST_PATH 	= test
TMP_PATH 	= tmp
RELEASE 	= $(TARGET)
LIB 		= lib$(TARGET).a
# ------------------------- #

rwildcard = $(foreach d, $(wildcard $1*), $(call rwildcard,$d/,$2) \
						$(filter $2, $d))

ifneq ($(strip $(SRC_PATH)),)
    SRC += $(call rwildcard, $(SRC_PATH), %.$(SRC_EXT))
endif

# SRC = $(call rwildcard, $(SRC_PATH), %.$(SRC_EXT))
OBJS = $(SRC:$(SRC_EXT)=o)

ifeq ($(ARCH),x86_64)
	DEFINES += -DCONFIG_X86
	OBJS	+= hw/i8042.o
	OBJS	+= hw/serial.o
	OBJS	+= arch/x86_64/boot.o
	OBJS	+= arch/x86_64/cpuid.o
	OBJS	+= arch/x86_64/interrupt.o
	OBJS	+= arch/x86_64/ioport.o
	OBJS	+= arch/x86_64/irq.o
	OBJS	+= arch/x86_64/kvm.o
	OBJS	+= arch/x86_64/kvm-cpu.o
	OBJS	+= arch/x86_64/mptable.o
# Exclude BIOS object files from header dependencies.
	OTHEROBJS	+= arch/x86_64/bios.o
	OTHEROBJS	+= arch/x86_64/bios/bios-rom.o
endif

# SIMPLE_CLIB_SRC = $(call rwildcard, include/simple-clib, %.$(SRC_EXT))
# OBJS += $(SIMPLE_CLIB_SRC:$(SRC_EXT)=o)

# VIRTIO_SRC = $(call rwildcard, virtio, %.$(SRC_EXT))
# OBJS += $(VIRTIO_SRC:$(SRC_EXT)=o)

# DISK_SRC = $(call rwildcard, disk, %.$(SRC_EXT))
# OBJS += $(DISK_SRC:$(SRC_EXT)=o)

# NET_SRC = $(call rwildcard, net, %.$(SRC_EXT))
# OBJS += $(NET_SRC:$(SRC_EXT)=o)

comma = ,
# The dependency file for the current target
depfile = $(subst $(comma),_,$(dir $@).$(notdir $@).d)

DEPS	:= $(foreach obj,$(OBJS) $(OBJS_DYNOPT) $(OTHEROBJS) $(GUEST_OBJS),\
		$(subst $(comma),_,$(dir $(obj)).$(notdir $(obj)).d))

CFLAGS  += $(DEFINES)
c_flags	= -Wp,-MD,$(depfile) -Wp,-MT,$@ $(CFLAGS)

ifeq ($(MAKECMDGOALS),debug)
CFLAGS+=-g
endif

PROGRAM = $(TARGET)

all: $(PROGRAM)

debug: all

$(PROGRAM): $(OBJS) $(OBJS_DYNOPT) $(OTHEROBJS) $(GUEST_OBJS)
	$(E) -e "  LINK    \033[1;32m" $@ "\033[0m"
	$(Q) $(CC) $(CFLAGS) $(OBJS) $(OBJS_DYNOPT) $(OTHEROBJS) $(GUEST_OBJS) $(LDFLAGS) $(LIBS) $(LIBS_DYNOPT) $(LIBFDT_STATIC) -o $@

$(OBJS):
%.o: %.c
ifeq ($(C),1)
	$(E) "  CHECK   " $@
	$(Q) $(CHECK) -c $(CFLAGS) $(CFLAGS_DYNOPT) $< -o $@
endif
	$(E) "  CC      " $@
	$(Q) $(CC) -c $(c_flags) $(CFLAGS_DYNOPT) $< -o $@

include arch/$(ARCH)/Makefile

# ------------------------- #
#          使用方法
# ------------------------- #
.PHONY: clean distclean lib release tar all test

install: all
	$(E) "  INSTALL"
	$(Q) $(INSTALL) -d -m 755 '$(DESTDIR_SQ)$(bindir_SQ)' 
	$(Q) $(INSTALL) $(PROGRAM) '$(DESTDIR_SQ)$(bindir_SQ)' 
.PHONY: install


clean:
	$(E) "  CLEAN"
	$(Q) rm -f $(DEPS) $(STATIC_DEPS) $(OBJS) $(OTHEROBJS) $(OBJS_DYNOPT) $(STATIC_OBJS) $(PROGRAM) $(PROGRAM_ALIAS) $(PROGRAM)-static $(GUEST_INIT) $(GUEST_PRE_INIT) $(GUEST_OBJS)
	$(Q) rm -f arch/x86_64/bios/*.bin
	$(Q) rm -f arch/x86_64/bios/*.elf
	$(Q) rm -f arch/x86_64/bios/*.o
	$(Q) rm -f arch/x86_64/bios/bios-rom.h
	$(Q) rm -f $(PROGRAM)
release:
	$(MAKE) -j4
	mkdir $(RELEASE)
	@cp $(EXE) $(RELEASE)/ 
	tar -cvf $(TARGET).tar $(RELEASE)/

help:
	$(E) ""
	$(E) "  [$(TARGET) compile help]"
	$(E) ""
	$(E) "    make              编译"
	$(E) "    make help         帮助信息"
	$(E) "    make clean        清除编译文件"
	$(E) "    make all          编译"
	$(E) "    make install      安装"
	$(E) "    make debug        调试模式"
	$(E) "    make release      打包"
	$(E) ""

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
-include $(STATIC_DEPS)
endif
