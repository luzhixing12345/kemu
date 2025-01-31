
#pragma once

#include <clib/clib.h>
#include <kvm/kvm.h>

#include "kvm/disk-image.h"

void *kvm_cpu_thread(void *arg);

int vm_init(struct kvm *vm);
int vm_exit(struct kvm *vm);
int vm_validate_cfg(struct kvm_config *config);
int vm_run(struct kvm *vm);