#ifndef KVM__KVM_CPU_ARCH_H
#define KVM__KVM_CPU_ARCH_H

#include <linux/kvm.h> /* for struct kvm_regs */
#include <pthread.h>

#include "kvm/kvm.h" /* for kvm_emulate_{mm}io() */

struct kvm;

struct kvm_cpu {
    pthread_t thread; /* VCPU thread */

    unsigned long cpu_id;

    struct kvm *kvm; /* parent KVM */
    int vcpu_fd;     /* For VCPU ioctls() */
    struct kvm_run *kvm_run;
    struct kvm_cpu_task *task;

    struct kvm_regs regs;

    u8 is_running;
    u8 paused;
    u8 needs_nmi;

    struct kvm_coalesced_mmio_ring *ring;
};

/*
 * As these are such simple wrappers, let's have them in the header so they'll
 * be cheaper to call:
 */
static inline bool kvm_cpu__emulate_io(struct kvm_cpu *vcpu, u16 port, void *data, int direction, int size, u32 count) {
    return kvm_emulate_io(vcpu, port, data, direction, size, count);
}

static inline bool kvm_cpu__emulate_mmio(struct kvm_cpu *vcpu, u64 phys_addr, u8 *data, u32 len, u8 is_write) {
    return kvm_emulate_mmio(vcpu, phys_addr, data, len, is_write);
}

#endif /* KVM__KVM_CPU_ARCH_H */
