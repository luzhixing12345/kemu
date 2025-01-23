#ifndef KVM__IOEVENTFD_H
#define KVM__IOEVENTFD_H

#include <linux/list.h>
#include <linux/types.h>
#include <sys/eventfd.h>
#include <vm/vm.h>

#include "kvm/util.h"

struct kvm;

struct ioevent {
    u64 io_addr;
    u8 io_len;
    void (*fn)(struct kvm *kvm, void *ptr);
    struct kvm *fn_kvm;
    void *fn_ptr;
    int fd;
    u64 datamatch;
    u32 flags;

    struct list_head list;
};

#define IOEVENTFD_FLAG_PIO       (1 << 0)
#define IOEVENTFD_FLAG_USER_POLL (1 << 1)

int ioeventfd_init(struct vm *vm);
int ioeventfd_exit(struct vm *vm);
int ioeventfd__add_event(struct ioevent *ioevent, int flags);
int ioeventfd__del_event(u64 addr, u64 datamatch);

#endif
