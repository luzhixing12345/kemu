#include <asm-generic/errno-base.h>
#include <clib/clib.h>
#include <linux/err.h>
#include <mntent.h>

#include "kvm/disk-image.h"

/*
 * raw image and blk dev are similar, so reuse raw image ops.
 */
static struct disk_image_operations blk_dev_ops = {
    .read = raw_image__read,
    .write = raw_image__write,
    .wait = raw_image__wait,
    .async = true,
};

bool is_blkdev(int fd, struct stat *st) {
    return S_ISBLK(st->st_mode);
}

int blkdev_probe(struct disk_image *disk, int fd, int readonly) {
    int r;
    u64 size;
    disk->readonly = readonly;
    /*
     * Be careful! We are opening host block device!
     * Open it readonly since we do not want to break user's data on disk.
     */

    if (ioctl(fd, BLKGETSIZE64, &size) < 0) {
        r = -errno;
        close(fd);
        ERR("Failed to get size of block device %s: %s", disk->disk_path, strerror(errno));
        return r;
    }

    /*
     * FIXME: This will not work on 32-bit host because we can not
     * mmap large disk. There is not enough virtual address space
     * in 32-bit host. However, this works on 64-bit host.
     */
    return disk_image_new(disk, fd, size, &blk_dev_ops, DISK_IMAGE_REGULAR);
}
