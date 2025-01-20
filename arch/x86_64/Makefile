
#
# BIOS assembly weirdness
#
BIOS_CFLAGS += -m32
BIOS_CFLAGS += -march=i386
BIOS_CFLAGS += -mregparm=3

BIOS_CFLAGS += -fno-stack-protector
BIOS_CFLAGS += -fno-pic

ARCH_DIR = arch/$(ARCH)

$(ARCH_DIR)/bios.o: $(ARCH_DIR)/bios/bios.bin $(ARCH_DIR)/bios/bios-rom.h

$(ARCH_DIR)/bios/bios.bin.elf: $(ARCH_DIR)/bios/entry.S $(ARCH_DIR)/bios/e820.c $(ARCH_DIR)/bios/int10.c $(ARCH_DIR)/bios/int15.c $(ARCH_DIR)/bios/rom.ld.S
	$(E) "  CC       $(ARCH_DIR)/bios/memcpy.o"
	$(Q) $(CC) -include $(ARCH_DIR)/code16gcc.h $(CFLAGS) $(BIOS_CFLAGS) -c $(ARCH_DIR)/bios/memcpy.c -o $(ARCH_DIR)/bios/memcpy.o
	$(E) "  CC       $(ARCH_DIR)/bios/e820.o"
	$(Q) $(CC) -include $(ARCH_DIR)/code16gcc.h $(CFLAGS) $(BIOS_CFLAGS) -c $(ARCH_DIR)/bios/e820.c -o $(ARCH_DIR)/bios/e820.o
	$(E) "  CC       $(ARCH_DIR)/bios/int10.o"
	$(Q) $(CC) -include $(ARCH_DIR)/code16gcc.h $(CFLAGS) $(BIOS_CFLAGS) -c $(ARCH_DIR)/bios/int10.c -o $(ARCH_DIR)/bios/int10.o
	$(E) "  CC       $(ARCH_DIR)/bios/int15.o"
	$(Q) $(CC) -include $(ARCH_DIR)/code16gcc.h $(CFLAGS) $(BIOS_CFLAGS) -c $(ARCH_DIR)/bios/int15.c -o $(ARCH_DIR)/bios/int15.o
	$(E) "  CC       $(ARCH_DIR)/bios/entry.o"
	$(Q) $(CC) $(CFLAGS) $(BIOS_CFLAGS) -c $(ARCH_DIR)/bios/entry.S -o $(ARCH_DIR)/bios/entry.o
	$(E) "  LD      " $@
	$(Q) $(LD) -T $(ARCH_DIR)/bios/rom.ld.S -o $(ARCH_DIR)/bios/bios.bin.elf $(ARCH_DIR)/bios/memcpy.o $(ARCH_DIR)/bios/entry.o $(ARCH_DIR)/bios/e820.o $(ARCH_DIR)/bios/int10.o $(ARCH_DIR)/bios/int15.o

$(ARCH_DIR)/bios/bios.bin: $(ARCH_DIR)/bios/bios.bin.elf
	$(E) "  OBJCOPY " $@
	$(Q) $(OBJCOPY) -O binary -j .text $(ARCH_DIR)/bios/bios.bin.elf $(ARCH_DIR)/bios/bios.bin

$(ARCH_DIR)/bios/bios-rom.o: $(ARCH_DIR)/bios/bios-rom.S $(ARCH_DIR)/bios/bios.bin $(ARCH_DIR)/bios/bios-rom.h
	$(E) "  CC      " $@
	$(Q) $(CC) -c $(CFLAGS) $(ARCH_DIR)/bios/bios-rom.S -o $(ARCH_DIR)/bios/bios-rom.o

$(ARCH_DIR)/bios/bios-rom.h: $(ARCH_DIR)/bios/bios.bin.elf
	$(E) "  NM      " $@
	$(Q) cd $(ARCH_DIR)/bios && sh gen-offsets.sh > bios-rom.h && cd ..