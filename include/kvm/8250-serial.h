
#pragma once

#include <vm/vm.h>

int serial8250_init(struct vm *vm);
int serial8250_exit(struct vm *vm);
void serial8250__update_consoles(struct kvm *kvm);
void serial8250__inject_sysrq(struct kvm *kvm, char sysrq);
