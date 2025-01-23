#ifndef KVM__BLK_VIRTIO_H
#define KVM__BLK_VIRTIO_H

#include <vm/vm.h>

#include "kvm/disk-image.h"

int virtio_blk_init(struct vm *vm);
int virtio_blk_exit(struct vm *vm);
void virtio_blk_complete(void *param, long len);

#endif /* KVM__BLK_VIRTIO_H */
