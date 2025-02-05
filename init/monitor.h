
#pragma once

#include <kvm/kvm.h>

void monitor_start(struct kvm *kvm, int term);
void monitor_end(struct kvm *kvm, int term);
void monitor_add_cmdchar(char c, int term);