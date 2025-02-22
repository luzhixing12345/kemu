#include <kvm/assembly.h>

	.org 0
#ifdef CONFIG_X86_64
	.code64
#else
	.code32
#endif

GLOBAL(bios_rom)
	.incbin "arch/x86_64/bios/bios.bin"
END(bios_rom)

/*
 * Add this section to ensure final binary has a non-executable stack.
 */
.section .note.GNU-stack,"",@progbits
