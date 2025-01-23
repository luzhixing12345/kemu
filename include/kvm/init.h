#pragma once
#include <linux/types.h>
#include <vm/vm.h>

struct init_item {
    struct hlist_node n;
    const char *fn_name;
    int (*init)(struct vm *);
};

typedef enum {
    MODULE_CORE,
    MODULE_BASE,
    MODULE_DEV_BASE,
    MODULE_DEV,
    MODULE_VIRTIO_DEV,
    MODULE_FIRMWARE,
    MODULE_LATE,
    MODULE_MAX
} init_type;

int init_list_init(struct vm *vm);
int init_list_exit(struct vm *vm);

int init_list_add(struct init_item *t, int (*init)(struct vm *), init_type type, const char *name);
int exit_list_add(struct init_item *t, int (*init)(struct vm *), init_type type, const char *name);

#define __init_list_add(cb, l)                                    \
    static void __attribute__((constructor)) __init__##cb(void) { \
        static char name[] = #cb;                                 \
        static struct init_item t;                                \
        init_list_add(&t, cb, l, name);                           \
    }

#define __exit_list_add(cb, l)                                   \
    static void __attribute__((constructor)) __init__##cb(void) { \
        static char name[] = #cb;                                \
        static struct init_item t;                               \
        exit_list_add(&t, cb, l, name);                          \
    }

#define core_init(cb)       __init_list_add(cb, MODULE_CORE)
#define base_init(cb)       __init_list_add(cb, MODULE_BASE)
#define dev_base_init(cb)   __init_list_add(cb, MODULE_DEV_BASE)
#define dev_init(cb)        __init_list_add(cb, MODULE_DEV)
#define virtio_dev_init(cb) __init_list_add(cb, MODULE_VIRTIO_DEV)
#define firmware_init(cb)   __init_list_add(cb, MODULE_FIRMWARE)
#define late_init(cb)       __init_list_add(cb, MODULE_LATE)

#define core_exit(cb)       __exit_list_add(cb, MODULE_CORE)
#define base_exit(cb)       __exit_list_add(cb, MODULE_BASE)
#define dev_base_exit(cb)   __exit_list_add(cb, MODULE_DEV_BASE)
#define dev_exit(cb)        __exit_list_add(cb, MODULE_DEV)
#define virtio_dev_exit(cb) __exit_list_add(cb, MODULE_VIRTIO_DEV)
#define firmware_exit(cb)   __exit_list_add(cb, MODULE_FIRMWARE)
#define late_exit(cb)       __exit_list_add(cb, MODULE_LATE)