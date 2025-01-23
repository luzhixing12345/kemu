#ifndef KVM_MPTABLE_H_
#define KVM_MPTABLE_H_

#include <vm/vm.h>

int mptable_init(struct vm *vm);
int mptable_exit(struct vm *vm);

#endif /* KVM_MPTABLE_H_ */
