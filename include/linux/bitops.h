#ifndef _KVM_LINUX_BITOPS_H_
#define _KVM_LINUX_BITOPS_H_

#include <asm/hweight.h>
#include <linux/compiler.h>
#include <linux/kernel.h>

#define BITS_PER_BYTE     8
#define BITS_PER_LONG     (BITS_PER_BYTE * sizeof(long))
#define BITS_TO_LONGS(nr) DIV_ROUND_UP(nr, BITS_PER_LONG)

#define BIT_WORD(nr)      ((nr) / BITS_PER_LONG)

static inline void set_bit(int nr, unsigned long *addr) {
    addr[nr / BITS_PER_LONG] |= 1UL << (nr % BITS_PER_LONG);
}

static inline void clear_bit(int nr, unsigned long *addr) {
    addr[nr / BITS_PER_LONG] &= ~(1UL << (nr % BITS_PER_LONG));
}

static __always_inline int test_bit(unsigned int nr, const unsigned long *addr) {
    return ((1UL << (nr % BITS_PER_LONG)) & (((unsigned long *)addr)[nr / BITS_PER_LONG])) != 0;
}

static inline unsigned long hweight_long(unsigned long w) {
    return sizeof(w) == 4 ? hweight32(w) : hweight64(w);
}

#endif
