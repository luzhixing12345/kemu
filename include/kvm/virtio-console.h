#pragma once
#include <vm/vm.h>

int virtio_console_init(struct vm *vm);
void virtio_console__inject_interrupt(struct kvm *kvm);
int virtio_console_exit(struct vm *vm);
