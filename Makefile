# ------------------------- #
#          VERSION          #
# ------------------------- #
VERSION 		:= 0
PATCHLEVEL 		:= 0
SUBLEVEL 		:= 1

# default: [all, lib, each]
# ------------------------- #
# all: compile all the files under SRC_PATH and generate TARGET
#      $ gcc $(SRC_PATH)/*.$(SRC_EXT) -o $(TARGET)
# lib: compile all the files under SRC_PATH and generate LIBNAME(a)
#      $ gcc $(SRC_PATH)/*.$(SRC_EXT) -o *.o
#      $ ar rcs lib$(LIBNAME).a *.o
# each: compile each file under SRC_PATH and generate each file
#      $ gcc $(SRC_PATH)/file1.$(SRC_EXT) -o file1
#      $ gcc $(SRC_PATH)/file2.$(SRC_EXT) -o file2
#      ...
# ------------------------- #
default: all

# ------------------------- #
#          PROJECT          #
# ------------------------- #
CC          	:= gcc
TARGET      	:= kemu
LIBNAME     	:= 
SRC_PATH    	:= init virtio disk net util vfio
SRC_EXT     	:= c

# Translate uname -m into ARCH string
ARCH ?= $(shell uname -m | sed -e s/i.86/i386/ -e s/ppc.*/powerpc/ \
	  -e s/armv.*/arm/ -e s/aarch64.*/arm64/ -e s/mips64/mips/ \
	  -e s/riscv64/riscv/ -e s/riscv32/riscv/)

# ------------------------- #
#          FLAGS            #
# ------------------------- #
CFLAGS 			:= -DCONFIG_VERSION=\"$(VERSION).$(PATCHLEVEL).$(SUBLEVEL)\" -g
INCLUDE_PATH 	:= -Iinclude -Iarch/$(ARCH)/include -Iinclude/clib
LDFLAGS 		:= -lpthread -lrt -Linclude/clib/clib -lclib
DEFINES     	:= -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -DBUILD_ARCH='"$(ARCH)"' -DKVMTOOLS_VERSION='"$(KVMTOOLS_VERSION)"'
THIRD_LIB   	:= 
# ------------------------- #
CFLAGS += $(INCLUDE_PATH)

# ------------------------- #
# STATIC_LIB 		:= $(SRC_PATH)/lib$(LIBNAME).a

# ------------------------- #
#          BINARIES         #
# ------------------------- #
AR          := ar
LD          := ld
FIND	    := find
CSCOPE	    := cscope
TAGS	    := ctags
INSTALL     := install
CHECK       := check
OBJCOPY		:= objcopy

# ------------------------- #
#          CLIB BUILD       #
# ------------------------- #
CLIB_DIR := include/clib
CLIB_LIB := $(CLIB_DIR)/clib/libclib.a

.PHONY: build-clib
build-clib:
	$(E) "  BUILDING CLIB"
	$(Q) $(MAKE) -C $(CLIB_DIR) lib

$(CLIB_LIB): build-clib


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
WARNING += -Wno-system-headers
WARNING += -Wredundant-decls
WARNING += -Wsign-compare
WARNING += -Wundef
WARNING += -Wvolatile-register-var
WARNING += -Wno-format-nonliteral
WARNING += -Wno-pedantic
# if SRC_EXT is c
ifeq ($(SRC_EXT),c)
WARNING += -Wnested-externs
WARNING += -Wno-discarded-qualifiers
endif
CFLAGS	+= $(WARNING)

# ------------------------- #

ifeq ($(strip $(V)),)
	ifeq ($(findstring s,$(filter-out --%,$(firstword $(MAKEFLAGS)))),)
		E = @printf
	else
		E = @\#
	endif
	Q = @
else
	E = @\#
	Q =
endif
export E Q

# Translate uname -m into ARCH string
ARCH ?= $(shell uname -m | sed -e s/i.86/i386/ -e s/ppc.*/powerpc/ \
	  -e s/armv.*/arm/ -e s/aarch64.*/arm64/ -e s/mips64/mips/ \
	  -e s/riscv64/riscv/ -e s/riscv32/riscv/)

ifneq ($(THIRD_LIB),)
CFLAGS 	+= $(shell pkg-config --cflags $(THIRD_LIB))
LDFLAGS += $(shell pkg-config --libs $(THIRD_LIB))
endif

rwildcard = $(foreach d, $(filter-out .., $(wildcard $1*)), \
             $(call rwildcard,$d/,$2) $(filter $2, $d))

ifneq ($(strip $(SRC_PATH)),)
    SRC += $(call rwildcard, $(SRC_PATH), %.$(SRC_EXT))
endif

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

comma = ,
# The dependency file for the current target
depfile = $(subst $(comma),_,$(dir $@).$(notdir $@).d)

DEPS	:= $(foreach obj,$(OBJS) $(OBJS_DYNOPT) $(OTHEROBJS) $(GUEST_OBJS),\
		$(subst $(comma),_,$(dir $(obj)).$(notdir $(obj)).d))

CFLAGS  += $(DEFINES)
c_flags	= -Wp,-MD,$(depfile) -Wp,-MT,$@ $(CFLAGS)

ifeq ($(MAKECMDGOALS),debug)
CFLAGS += -g
endif

PROGRAM = $(TARGET)

all: $(PROGRAM)
.PHONY: all

debug: default
.PHONY: debug

# compile program bin
$(PROGRAM): $(CLIB_LIB) $(OBJS) $(OBJS_DYNOPT) $(OTHEROBJS) $(GUEST_OBJS)
	$(E) "  LINK    \033[1;32m%s\033[0m\n" $@
	$(Q) $(CC) $(CFLAGS) $(OBJS) $(OBJS_DYNOPT) $(OTHEROBJS) $(GUEST_OBJS) $(LDFLAGS) $(LIBS_DYNOPT) $(LIBFDT_STATIC) -o $@
	$(E) "  binary program $(PROGRAM) is ready.\n"

# compile static lib
$(STATIC_LIB): $(OBJS) $(OTHEROBJS) $(GUEST_OBJS)
	$(E) "  LINK    \033[1;32m%s\033[0m\n" $@
	$(Q) $(AR) rcs $@ $(OBJS) $(OTHEROBJS) $(GUEST_OBJS)
	$(E) "  static library $(STATIC_LIB) is ready.\n"

# compile dynamic lib
# $(DYNAMIC_LIB): $(OBJS) $(OTHEROBJS) $(GUEST_OBJS)
# 	$(E) "  LINK    \033[1;32m%s\033[0m\n" $@
# 	$(Q) $(LD) -shared $(OBJS) $(OTHEROBJS) $(GUEST_OBJS) -o $@
# 	$(E) "  dynamic library $(DYNAMIC_LIB) is ready.\n"

# compile both static and dynamic lib
lib: $(STATIC_LIB)
.PHONY: lib

EXECUTABLES = $(OBJS:.o=)
%: %.o
	$(E) "  LINK    \033[1;32m%s\033[0m\n" $@
	$(Q) $(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

each: $(EXECUTABLES)
.PHONY: each


$(OBJS):
%.o: %.$(SRC_EXT)
ifeq ($(C),1)
	$(E) "  CHECK   %s\n" $@
	$(Q) $(CHECK) -c $(CFLAGS) $(CFLAGS_DYNOPT) $< -o $@
endif
	$(E) "  CC      %s\n" $@
	$(Q) $(CC) -c $(c_flags) $(CFLAGS_DYNOPT) $< -o $@
include arch/$(ARCH)/Makefile
# ------------------------- #
#          使用方法
# ------------------------- #
.PHONY: clean distclean lib release tar all test

install: all
	$(E) "  INSTALL\n"
	$(Q) $(INSTALL) -d -m 755 '$(DESTDIR_SQ)$(bindir_SQ)' 
	$(Q) $(INSTALL) $(PROGRAM) '$(DESTDIR_SQ)$(bindir_SQ)' 
.PHONY: install


clean:
	$(E) "  CLEAN\n"
	$(Q) rm -f $(DEPS) $(STATIC_DEPS) $(OBJS) $(OTHEROBJS) $(OBJS_DYNOPT) $(STATIC_OBJS) $(PROGRAM) $(PROGRAM_ALIAS) $(GUEST_INIT) $(GUEST_PRE_INIT) $(GUEST_OBJS)
	$(Q) rm -f $(PROGRAM) $(EXECUTABLES)
	$(Q) rm -f $(STATIC_LIB)
	$(Q) rm -f $(DYNAMIC_LIB)
	$(E) "  CLEAN CLIB"
	$(Q) $(MAKE) -C $(CLIB_DIR) clean
release:
	$(MAKE) -j4
	mkdir $(RELEASE)
	@cp $(EXE) $(RELEASE)/ 
	tar -cvf $(TARGET).tar $(RELEASE)/

# 输出配置信息, 包括 CFLAGS, LDFLAGS, LIBS
config:
	$(E) "CONFIG\n"
	$(E) "  [SRC FILES]: %s\n" "$(shell echo $(SRC) | tr '\n' ' ')"
	$(E) "  [CFLAGS]: %s\n" "$(CFLAGS)"
	$(E) "  [LDFLAGS]: %s\n" "$(LDFLAGS)"

help:
	$(E) "\n"
	$(E) "  [%s compile help]\n" $(TARGET)
	$(E) "\n"
	$(E) "    make              编译\n"
	$(E) "    make help         帮助信息\n"
	$(E) "    make clean        清除编译文件\n"
	$(E) "    make config       查看配置信息\n"
	$(E) "    make install      安装\n"
	$(E) "    make debug        调试模式\n"
	$(E) "\n"

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
-include $(STATIC_DEPS)
endif
