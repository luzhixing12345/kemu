#ifndef KVM__KVM_H
#define KVM__KVM_H

#include <limits.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/prctl.h>
#include <time.h>

#include "kvm/kvm-arch.h"
#include "kvm/kvm-config.h"
#include "kvm/mutex.h"
#include "kvm/util-init.h"

#define SIGKVMEXIT        (SIGRTMIN + 0)
#define SIGKVMPAUSE       (SIGRTMIN + 1)
#define SIGKVMTASK        (SIGRTMIN + 2)

#define KVM_PID_FILE_PATH "/.lkvm/"
#define HOME_DIR          getenv("HOME")
#define KVM_BINARY_NAME   "lkvm"

#ifndef PAGE_SIZE
#define PAGE_SIZE (sysconf(_SC_PAGE_SIZE))
#endif

/*
 * We are reusing the existing DEVICE_BUS_MMIO and DEVICE_BUS_IOPORT constants
 * from kvm/devices.h to differentiate between registering an I/O port and an
 * MMIO region.
 * To avoid collisions with future additions of more bus types, we reserve
 * a generous 4 bits for the bus mask here.
 */
#define IOTRAP_BUS_MASK     0xf
#define IOTRAP_COALESCE     (1U << 4)

#define DEFINE_KVM_EXT(ext) .name = #ext, .code = ext

struct kvm_cpu;
typedef void (*mmio_handler_fn)(struct kvm_cpu *vcpu, u64 addr, u8 *data, u32 len, u8 is_write, void *ptr);

enum {
    KVM_VMSTATE_RUNNING,
    KVM_VMSTATE_PAUSED,
};

enum kvm_mem_type {
    KVM_MEM_TYPE_RAM = 1 << 0,
    KVM_MEM_TYPE_DEVICE = 1 << 1,
    KVM_MEM_TYPE_RESERVED = 1 << 2,
    KVM_MEM_TYPE_READONLY = 1 << 3,

    KVM_MEM_TYPE_ALL = KVM_MEM_TYPE_RAM | KVM_MEM_TYPE_DEVICE | KVM_MEM_TYPE_RESERVED | KVM_MEM_TYPE_READONLY
};

struct kvm_ext {
    const char *name;
    int code;
};

struct kvm_mem_bank {
    struct list_head list;
    u64 guest_phys_addr;
    void *host_addr;
    u64 size;
    enum kvm_mem_type type;
    u32 slot;
};

struct kvm {
    struct kvm_arch arch;
    struct kvm_config cfg;
    int kvm_fd;      /* For system ioctls(), i.e. /dev/kvm */
    int vm_fd;       /* For VM ioctls() */
    timer_t timerid; /* Posix timer for interrupts */

    int nrcpus; /* Number of cpus to run */
    struct kvm_cpu **cpus;

    u32 mem_slots; /* for KVM_SET_USER_MEMORY_REGION */
    u64 ram_size;  /* Guest memory size, in bytes */
    void *ram_start;
    u64 ram_pagesize;
    struct mutex mem_banks_lock;
    struct list_head mem_banks;

    bool nmi_disabled;
    bool msix_needs_devid;

    const char *vmlinux;
    struct disk_image *disks;
    int nr_disks;

    int vm_state;

#ifdef KVM_BRLOCK_DEBUG
    pthread_rwlock_t brlock_sem;
#endif
};

void kvm_set_dir(const char *fmt, ...);
const char *kvm_get_dir(void);

int kvm_init(struct kvm *kvm);
struct kvm *kvm_new(void);
int kvm_recommended_cpus(struct kvm *kvm);
int kvm_max_cpus(struct kvm *kvm);
int kvm_get_vm_type(struct kvm *kvm);
void kvm_init_ram(struct kvm *kvm);
int kvm_exit(struct kvm *kvm);
bool kvm_load_firmware(struct kvm *kvm, const char *firmware_filename);
bool kvm_load_kernel(struct kvm *kvm, const char *kernel_filename, const char *initrd_filename,
                     const char *kernel_cmdline);
int kvm_timer__init(struct kvm *kvm);
int kvm_timer__exit(struct kvm *kvm);
void kvm_irq_line(struct kvm *kvm, int irq, int level);
void kvm_irq_trigger(struct kvm *kvm, int irq);
bool kvm_emulate_io(struct kvm_cpu *vcpu, u16 port, void *data, int direction, int size, u32 count);
bool kvm_emulate_mmio(struct kvm_cpu *vcpu, u64 phys_addr, u8 *data, u32 len, u8 is_write);
int kvm_destroy_mem(struct kvm *kvm, u64 guest_phys, u64 size, void *userspace_addr);
int kvm_register_mem(struct kvm *kvm, u64 guest_phys, u64 size, void *userspace_addr, enum kvm_mem_type type);
static inline int kvm_register_ram(struct kvm *kvm, u64 guest_phys, u64 size, void *userspace_addr) {
    return kvm_register_mem(kvm, guest_phys, size, userspace_addr, KVM_MEM_TYPE_RAM);
}

static inline int kvm_register_dev_mem(struct kvm *kvm, u64 guest_phys, u64 size, void *userspace_addr) {
    return kvm_register_mem(kvm, guest_phys, size, userspace_addr, KVM_MEM_TYPE_DEVICE);
}

static inline int kvm_reserve_mem(struct kvm *kvm, u64 guest_phys, u64 size) {
    return kvm_register_mem(kvm, guest_phys, size, NULL, KVM_MEM_TYPE_RESERVED);
}

int __must_check kvm_register_iotrap(struct kvm *kvm, u64 phys_addr, u64 len, mmio_handler_fn mmio_fn, void *ptr,
                                     unsigned int flags);

static inline int __must_check kvm_register_mmio(struct kvm *kvm, u64 phys_addr, u64 phys_addr_len, bool coalesce,
                                                 mmio_handler_fn mmio_fn, void *ptr) {
    return kvm_register_iotrap(
        kvm, phys_addr, phys_addr_len, mmio_fn, ptr, DEVICE_BUS_MMIO | (coalesce ? IOTRAP_COALESCE : 0));
}
static inline int __must_check kvm_register_pio(struct kvm *kvm, u16 port, u16 len, mmio_handler_fn mmio_fn,
                                                void *ptr) {
    return kvm_register_iotrap(kvm, port, len, mmio_fn, ptr, DEVICE_BUS_IOPORT);
}

bool kvm_deregister_iotrap(struct kvm *kvm, u64 phys_addr, unsigned int flags);
static inline bool kvm_deregister_mmio(struct kvm *kvm, u64 phys_addr) {
    return kvm_deregister_iotrap(kvm, phys_addr, DEVICE_BUS_MMIO);
}
static inline bool kvm_deregister_pio(struct kvm *kvm, u16 port) {
    return kvm_deregister_iotrap(kvm, port, DEVICE_BUS_IOPORT);
}

void kvm_vm_exit(struct kvm *kvm);
void kvm_pause(struct kvm *kvm);
void kvm_continue(struct kvm *kvm);
void kvm_notify_paused(void);
int kvm_get_sock_by_instance(const char *name);
int kvm_enumerate_instances(int (*callback)(const char *name, int pid));
void kvm_remove_socket(const char *name);

void kvm_arch_validate_cfg(struct kvm *kvm);
void kvm_arch_set_cmdline(char *cmdline, bool video);
void kvm_arch_init(struct kvm *kvm);
u64 kvm_arch_default_ram_address(void);
void kvm_arch_delete_ram(struct kvm *kvm);
int kvm_arch_setup_firmware(struct kvm *kvm);
int kvm_arch_free_firmware(struct kvm *kvm);
bool kvm_arch_cpu_supports_vm(void);
void kvm_arch_read_term(struct kvm *kvm);

#ifdef ARCH_HAS_CFG_RAM_ADDRESS
static inline bool kvm_arch_has_cfg_ram_address(void) {
    return true;
}
#else
static inline bool kvm_arch_has_cfg_ram_address(void) {
    return false;
}
#endif

void *guest_flat_to_host(struct kvm *kvm, u64 offset);
u64 host_to_guest_flat(struct kvm *kvm, void *ptr);

bool kvm_arch_load_kernel_image(struct kvm *kvm, int fd_kernel, int fd_initrd, const char *kernel_cmdline);

#define add_read_only(type, str) (((type)&KVM_MEM_TYPE_READONLY) ? str " (read-only)" : str)
static inline const char *kvm_mem_type_to_string(enum kvm_mem_type type) {
    switch (type & ~KVM_MEM_TYPE_READONLY) {
        case KVM_MEM_TYPE_ALL:
            return "(all)";
        case KVM_MEM_TYPE_RAM:
            return add_read_only(type, "RAM");
        case KVM_MEM_TYPE_DEVICE:
            return add_read_only(type, "device");
        case KVM_MEM_TYPE_RESERVED:
            return add_read_only(type, "reserved");
    }

    return "???";
}

int kvm_for_each_mem_bank(struct kvm *kvm, enum kvm_mem_type type,
                          int (*fun)(struct kvm *kvm, struct kvm_mem_bank *bank, void *data), void *data);

/*
 * Debugging
 */
void kvm_dump_mem(struct kvm *kvm, unsigned long addr, unsigned long size, int debug_fd);

extern const char *kvm_exit_reasons[];

static inline bool host_ptr_in_ram(struct kvm *kvm, void *p) {
    return kvm->ram_start <= p && p < (kvm->ram_start + kvm->ram_size);
}

bool kvm_supports_extension(struct kvm *kvm, unsigned int extension);
bool kvm_supports_vm_extension(struct kvm *kvm, unsigned int extension);

static inline void kvm_set_thread_name(const char *name) {
    prctl(PR_SET_NAME, name);
}

#endif /* KVM__KVM_H */
