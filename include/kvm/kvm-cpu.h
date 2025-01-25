#ifndef KVM__KVM_CPU_H
#define KVM__KVM_CPU_H

#include <stdbool.h>
#include <vm/vm.h>

#include "kvm/kvm-cpu-arch.h"

struct kvm_cpu_task {
    void (*func)(struct kvm_cpu *vcpu, void *data);
    void *data;
};

int kvm_cpu_init(struct vm *vm);
int kvm_cpu_exit(struct vm *vm);
struct kvm_cpu *kvm_cpu__arch_init(struct kvm *kvm, unsigned long cpu_id);
void kvm_cpu_delete(struct kvm_cpu *vcpu);
void kvm_cpu__reset_vcpu(struct kvm_cpu *vcpu);
void kvm_cpu__setup_cpuid(struct kvm_cpu *vcpu);
void kvm_cpu__enable_singlestep(struct kvm_cpu *vcpu);
void kvm_cpu_run(struct kvm_cpu *vcpu);
int kvm_cpu_start(struct kvm_cpu *cpu);
void *kvm_cpu_thread(void *arg);
bool kvm_cpu__handle_exit(struct kvm_cpu *vcpu);
int kvm_cpu__get_endianness(struct kvm_cpu *vcpu);

int kvm_cpu__get_debug_fd(void);
void kvm_cpu_set_debug_fd(int fd);
void kvm_cpu_show_code(struct kvm_cpu *vcpu);
void kvm_cpu_show_registers(struct kvm_cpu *vcpu);
void kvm_cpu_show_page_tables(struct kvm_cpu *vcpu);
void kvm_cpu__arch_nmi(struct kvm_cpu *cpu);
void kvm_cpu__run_on_all_cpus(struct kvm *kvm, struct kvm_cpu_task *task);

#endif /* KVM__KVM_CPU_H */
