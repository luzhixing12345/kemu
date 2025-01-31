#include <linux/err.h>
#include <poll.h>
#include <stdbool.h>
#include <vm/vm.h>

#include "clib/log.h"
#include "kvm/disk-image.h"
#include "kvm/iovec.h"
#include "kvm/qcow.h"
#include "kvm/virtio-blk.h"
int debug_iodelay;

static int disk_image_close(struct disk_image *disk);

int disk_img_name_parser(const struct option *opt, const char *arg, int unset) {
    const char *cur;
    char *sep;
    struct kvm *kvm = opt->ptr;

    if (kvm->nr_disks >= MAX_DISK_IMAGES)
        die("Currently only 4 images are supported");

    kvm->cfg.disk_image[kvm->nr_disks].filename = arg;
    cur = arg;

    if (strncmp(arg, "scsi:", 5) == 0) {
        sep = strstr(arg, ":");
        kvm->cfg.disk_image[kvm->nr_disks].wwpn = sep + 1;

        /* Old invocation had two parameters. Ignore the second one. */
        sep = strstr(sep + 1, ":");
        if (sep) {
            *sep = 0;
            cur = sep + 1;
        }
    }

    do {
        sep = strstr(cur, ",");
        if (sep) {
            if (strncmp(sep + 1, "ro", 2) == 0)
                kvm->cfg.disk_image[kvm->nr_disks].readonly = true;
            else if (strncmp(sep + 1, "direct", 6) == 0)
                kvm->cfg.disk_image[kvm->nr_disks].direct = true;
            *sep = 0;
            cur = sep + 1;
        }
    } while (sep);

    kvm->nr_disks++;

    return 0;
}

int disk_image_new(struct disk_image *disk, int fd, u64 size, struct disk_image_operations *ops, int use_mmap) {
    int r;
    disk->fd = fd;
    disk->size = size;
    disk->ops = ops;

    if (use_mmap == DISK_IMAGE_MMAP) {
        /*
         * The write to disk image will be discarded
         */
        disk->priv = mmap(NULL, size, PROT_RW, MAP_PRIVATE | MAP_NORESERVE, fd, 0);
        if (disk->priv == MAP_FAILED) {
            r = -errno;
            goto err_free_disk;
        }
    }

    r = disk_aio_setup(disk);
    if (r)
        goto err_unmap_disk;

    return 0;

err_unmap_disk:
    if (disk->priv)
        munmap(disk->priv, size);
err_free_disk:
    free(disk);
    return r;
}

static int disk_image_open(struct disk_image *disk) {
    const char *disk_path = disk->disk_path;
    int direct = disk->direct;
    int readonly = disk->readonly;
    struct stat st;
    int fd, flags;
    int r;

    if (readonly)
        flags = O_RDONLY;
    else
        flags = O_RDWR;
    if (direct)
        flags |= O_DIRECT;

    if (stat(disk_path, &st) < 0) {
        ERR("Failed to stat %s: %s", disk_path, strerror(errno));
        return -errno;
    }

    fd = open(disk_path, flags);
    if (fd < 0) {
        ERR("Failed to open %s: %s", disk_path, strerror(errno));
        return -errno;
    }

    /* blk device ?*/
    if (is_blkdev(fd, &st)) {
        DEBUG("open blkdev %s", disk_path);
        if (is_mounted(&st)) {
            ERR("Block device %s is already mounted! Unmount before use.", disk_path);
            return -EBUSY;
        }
        r = blkdev_probe(disk, fd, true);
    } else if (is_qcow(fd)) {
        /* qcow image ?*/
        DEBUG("open qcow %s", disk_path);
        r = qcow_probe(disk, fd, true);
    } else {
        /* raw image ?*/
        DEBUG("open raw %s", disk_path);
        r = raw_image_probe(disk, fd, &st, readonly);
    }
    return r;
}

static int disk_image_open_all(struct kvm *kvm) {
    struct disk_image *disks = kvm->disks;

    for (int i = 0; i < kvm->nr_disks; i++) {
        DEBUG("open disk %s", disks[i].disk_path);
        if (disk_image_open(&disks[i]) < 0) {
            goto error;
        }
    }

    return 0;
error:

    return -1;
}

int disk_image__wait(struct disk_image *disk) {
    if (disk->ops->wait)
        return disk->ops->wait(disk);

    return 0;
}

int disk_image__flush(struct disk_image *disk) {
    if (disk->ops->flush)
        return disk->ops->flush(disk);

    return fsync(disk->fd);
}

static int disk_image_close(struct disk_image *disk) {
    /* If there was no disk image then there's nothing to do: */
    if (!disk)
        return 0;

    disk_aio_destroy(disk);

    if (disk->ops && disk->ops->close)
        return disk->ops->close(disk);

    if (disk->fd && close(disk->fd) < 0)
        WARNING("close() failed");

    return 0;
}

static int disk_image_close_all(struct disk_image *disks, int nr_disks) {
    while (nr_disks) disk_image_close(&disks[--nr_disks]);

    return 0;
}

/*
 * Fill iov with disk data, starting from sector 'sector'.
 * Return amount of bytes read.
 */
ssize_t disk_image__read(struct disk_image *disk, u64 sector, const struct iovec *iov, int iovcount, void *param) {
    ssize_t total = 0;

    if (debug_iodelay)
        msleep(debug_iodelay);

    if (disk->ops->read) {
        total = disk->ops->read(disk, sector, iov, iovcount, param);
        if (total < 0) {
            pr_info("disk_image__read error: total=%ld\n", (long)total);
            return total;
        }
    }

    if (!disk->async && disk->disk_req_cb)
        disk->disk_req_cb(param, total);

    return total;
}

/*
 * Write iov to disk, starting from sector 'sector'.
 * Return amount of bytes written.
 */
ssize_t disk_image__write(struct disk_image *disk, u64 sector, const struct iovec *iov, int iovcount, void *param) {
    ssize_t total = 0;

    if (debug_iodelay)
        msleep(debug_iodelay);

    if (disk->ops->write) {
        /*
         * Try writev based operation first
         */

        total = disk->ops->write(disk, sector, iov, iovcount, param);
        if (total < 0) {
            pr_info("disk_image__write error: total=%ld\n", (long)total);
            return total;
        }
    } else {
        /* Do nothing */
    }

    if (!disk->async && disk->disk_req_cb)
        disk->disk_req_cb(param, total);

    return total;
}

ssize_t disk_image__get_serial(struct disk_image *disk, struct iovec *iov, int iovcount, ssize_t len) {
    struct stat st;
    void *buf;
    int r;

    r = fstat(disk->fd, &st);
    if (r)
        return r;

    buf = malloc(len);
    if (!buf)
        return -ENOMEM;

    len = snprintf(buf,
                   len,
                   "%llu%llu%llu",
                   (unsigned long long)st.st_dev,
                   (unsigned long long)st.st_rdev,
                   (unsigned long long)st.st_ino);
    if (len < 0 || (size_t)len > iov_size(iov, iovcount)) {
        free(buf);
        return -ENOMEM;
    }

    memcpy_toiovec(iov, buf, len);
    free(buf);
    return len;
}

void disk_image__set_callback(struct disk_image *disk, void (*disk_req_cb)(void *param, long len)) {
    disk->disk_req_cb = disk_req_cb;
}

int disk_image_init(struct kvm *kvm) {
    disk_image_open_all(kvm);
    return 0;
}
dev_base_init(disk_image_init);

int disk_image_exit(struct kvm *kvm) {
    return disk_image_close_all(kvm->disks, kvm->nr_disks);
}
dev_base_exit(disk_image_exit);
