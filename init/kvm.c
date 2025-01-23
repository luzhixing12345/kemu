#include <asm/unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <kvm/kvm-cpu.h>
#include <kvm/kvm-ipc.h>
#include <kvm/mutex.h>
#include <kvm/read-write.h>
#include <kvm/strbuf.h>
#include <kvm/term.h>
#include <kvm/util.h>
#include <limits.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/kvm.h>
#include <linux/list.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include <vm/vm.h>
#include <simple-clib/logging.h>

#define DEFINE_KVM_EXIT_REASON(reason) [reason] = #reason

const char *kvm_exit_reasons[] = {
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_UNKNOWN),    DEFINE_KVM_EXIT_REASON(KVM_EXIT_EXCEPTION),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_IO),         DEFINE_KVM_EXIT_REASON(KVM_EXIT_HYPERCALL),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_DEBUG),      DEFINE_KVM_EXIT_REASON(KVM_EXIT_HLT),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_MMIO),       DEFINE_KVM_EXIT_REASON(KVM_EXIT_IRQ_WINDOW_OPEN),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_SHUTDOWN),   DEFINE_KVM_EXIT_REASON(KVM_EXIT_FAIL_ENTRY),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_INTR),       DEFINE_KVM_EXIT_REASON(KVM_EXIT_SET_TPR),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_TPR_ACCESS), DEFINE_KVM_EXIT_REASON(KVM_EXIT_S390_SIEIC),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_S390_RESET), DEFINE_KVM_EXIT_REASON(KVM_EXIT_DCR),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_NMI),        DEFINE_KVM_EXIT_REASON(KVM_EXIT_INTERNAL_ERROR),
#ifdef CONFIG_PPC64
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_PAPR_HCALL),
#endif
};

static int pause_event;
static DEFINE_MUTEX(pause_lock);
extern struct kvm_ext kvm_req_ext[];

static char kvm_dir[PATH_MAX];

extern __thread struct kvm_cpu *current_kvm_cpu;

static int set_dir(const char *fmt, va_list args) {
    char tmp[PATH_MAX];

    vsnprintf(tmp, sizeof(tmp), fmt, args);

    mkdir(tmp, 0777);

    if (!realpath(tmp, kvm_dir))
        return -errno;

    strcat(kvm_dir, "/");

    return 0;
}

void kvm_set_dir(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    set_dir(fmt, args);
    va_end(args);
}

const char *kvm_get_dir(void) {
    return kvm_dir;
}

bool kvm_supports_vm_extension(struct kvm *kvm, unsigned int extension) {
    static int supports_vm_ext_check = 0;
    int ret;

    switch (supports_vm_ext_check) {
        case 0:
            ret = ioctl(kvm->kvm_fd, KVM_CHECK_EXTENSION, KVM_CAP_CHECK_EXTENSION_VM);
            if (ret <= 0) {
                supports_vm_ext_check = -1;
                return false;
            }
            supports_vm_ext_check = 1;
            /* fall through */
        case 1:
            break;
        case -1:
            return false;
    }

    ret = ioctl(kvm->vm_fd, KVM_CHECK_EXTENSION, extension);
    if (ret < 0)
        return false;

    return ret;
}

bool kvm_supports_extension(struct kvm *kvm, unsigned int extension) {
    int ret;

    ret = ioctl(kvm->kvm_fd, KVM_CHECK_EXTENSION, extension);
    if (ret < 0)
        return false;

    return ret;
}

static int kvm_check_extensions(struct kvm *kvm) {
    int i;

    for (i = 0;; i++) {
        if (!kvm_req_ext[i].name)
            break;
        if (!kvm_supports_extension(kvm, kvm_req_ext[i].code)) {
            pr_err("Unsupported KVM extension detected: %s", kvm_req_ext[i].name);
            return -i;
        }
    }

    return 0;
}

int kvm_exit(struct vm *vm) {
    struct kvm_mem_bank *bank, *tmp;
    struct kvm *kvm = &vm->kvm;

    kvm_arch_delete_ram(kvm);

    list_for_each_entry_safe(bank, tmp, &kvm->mem_banks, list) {
        list_del(&bank->list);
        free(bank);
    }

    free(kvm);
    return 0;
}
core_exit(kvm_exit);

int kvm_destroy_mem(struct kvm *kvm, u64 guest_phys, u64 size, void *userspace_addr) {
    struct kvm_userspace_memory_region mem;
    struct kvm_mem_bank *bank;
    int ret;

    mutex_lock(&kvm->mem_banks_lock);
    list_for_each_entry(bank, &kvm->mem_banks, list) if (bank->guest_phys_addr == guest_phys && bank->size == size &&
                                                         bank->host_addr == userspace_addr) break;

    if (&bank->list == &kvm->mem_banks) {
        pr_err("Region [%llx-%llx] not found", guest_phys, guest_phys + size - 1);
        ret = -EINVAL;
        goto out;
    }

    if (bank->type == KVM_MEM_TYPE_RESERVED) {
        pr_err("Cannot delete reserved region [%llx-%llx]", guest_phys, guest_phys + size - 1);
        ret = -EINVAL;
        goto out;
    }

    mem = (struct kvm_userspace_memory_region){
        .slot = bank->slot,
        .guest_phys_addr = guest_phys,
        .memory_size = 0,
        .userspace_addr = (unsigned long)userspace_addr,
    };

    ret = ioctl(kvm->vm_fd, KVM_SET_USER_MEMORY_REGION, &mem);
    if (ret < 0) {
        ret = -errno;
        goto out;
    }

    list_del(&bank->list);
    free(bank);
    kvm->mem_slots--;
    ret = 0;

out:
    mutex_unlock(&kvm->mem_banks_lock);
    return ret;
}

int kvm_register_mem(struct kvm *kvm, u64 guest_phys, u64 size, void *userspace_addr, enum kvm_mem_type type) {
    struct kvm_userspace_memory_region mem;
    struct kvm_mem_bank *merged = NULL;
    struct kvm_mem_bank *bank;
    struct list_head *prev_entry;
    u32 slot;
    u32 flags = 0;
    int ret;

    mutex_lock(&kvm->mem_banks_lock);
    /* Check for overlap and find first empty slot. */
    slot = 0;
    prev_entry = &kvm->mem_banks;
    list_for_each_entry(bank, &kvm->mem_banks, list) {
        u64 bank_end = bank->guest_phys_addr + bank->size - 1;
        u64 end = guest_phys + size - 1;
        if (guest_phys > bank_end || end < bank->guest_phys_addr) {
            /*
             * Keep the banks sorted ascending by slot, so it's
             * easier for us to find a free slot.
             */
            if (bank->slot == slot) {
                slot++;
                prev_entry = &bank->list;
            }
            continue;
        }

        /* Merge overlapping reserved regions */
        if (bank->type == KVM_MEM_TYPE_RESERVED && type == KVM_MEM_TYPE_RESERVED) {
            bank->guest_phys_addr = min(bank->guest_phys_addr, guest_phys);
            bank->size = max(bank_end, end) - bank->guest_phys_addr + 1;

            if (merged) {
                /*
                 * This is at least the second merge, remove
                 * previous result.
                 */
                list_del(&merged->list);
                free(merged);
            }

            guest_phys = bank->guest_phys_addr;
            size = bank->size;
            merged = bank;

            /* Keep checking that we don't overlap another region */
            continue;
        }

        pr_err("%s region [%llx-%llx] would overlap %s region [%llx-%llx]",
               kvm_mem_type_to_string(type),
               guest_phys,
               guest_phys + size - 1,
               kvm_mem_type_to_string(bank->type),
               bank->guest_phys_addr,
               bank->guest_phys_addr + bank->size - 1);

        ret = -EINVAL;
        goto out;
    }

    if (merged) {
        ret = 0;
        goto out;
    }

    bank = malloc(sizeof(*bank));
    if (!bank) {
        ret = -ENOMEM;
        goto out;
    }

    INIT_LIST_HEAD(&bank->list);
    bank->guest_phys_addr = guest_phys;
    bank->host_addr = userspace_addr;
    bank->size = size;
    bank->type = type;
    bank->slot = slot;

    if (type & KVM_MEM_TYPE_READONLY)
        flags |= KVM_MEM_READONLY;

    if (type != KVM_MEM_TYPE_RESERVED) {
        mem = (struct kvm_userspace_memory_region){
            .slot = slot,
            .flags = flags,
            .guest_phys_addr = guest_phys,
            .memory_size = size,
            .userspace_addr = (unsigned long)userspace_addr,
        };

        ret = ioctl(kvm->vm_fd, KVM_SET_USER_MEMORY_REGION, &mem);
        if (ret < 0) {
            ret = -errno;
            goto out;
        }
    }

    list_add(&bank->list, prev_entry);
    kvm->mem_slots++;
    ret = 0;

out:
    mutex_unlock(&kvm->mem_banks_lock);
    return ret;
}

void *guest_flat_to_host(struct kvm *kvm, u64 offset) {
    struct kvm_mem_bank *bank;

    list_for_each_entry(bank, &kvm->mem_banks, list) {
        u64 bank_start = bank->guest_phys_addr;
        u64 bank_end = bank_start + bank->size;

        if (offset >= bank_start && offset < bank_end)
            return bank->host_addr + (offset - bank_start);
    }

    pr_warning("unable to translate guest address 0x%llx to host", (unsigned long long)offset);
    return NULL;
}

u64 host_to_guest_flat(struct kvm *kvm, void *ptr) {
    struct kvm_mem_bank *bank;

    list_for_each_entry(bank, &kvm->mem_banks, list) {
        void *bank_start = bank->host_addr;
        void *bank_end = bank_start + bank->size;

        if (ptr >= bank_start && ptr < bank_end)
            return bank->guest_phys_addr + (ptr - bank_start);
    }

    pr_warning("unable to translate host address %p to guest", ptr);
    return 0;
}

/*
 * Iterate over each registered memory bank. Call @fun for each bank with @data
 * as argument. @type is a bitmask that allows to filter banks according to
 * their type.
 *
 * If one call to @fun returns a non-zero value, stop iterating and return the
 * value. Otherwise, return zero.
 */
int kvm_for_each_mem_bank(struct kvm *kvm, enum kvm_mem_type type,
                          int (*fun)(struct kvm *kvm, struct kvm_mem_bank *bank, void *data), void *data) {
    int ret;
    struct kvm_mem_bank *bank;

    list_for_each_entry(bank, &kvm->mem_banks, list) {
        if (type != KVM_MEM_TYPE_ALL && !(bank->type & type))
            continue;

        ret = fun(kvm, bank, data);
        if (ret)
            break;
    }

    return ret;
}

int kvm_recommended_cpus(struct kvm *kvm) {
    int ret;

    ret = ioctl(kvm->kvm_fd, KVM_CHECK_EXTENSION, KVM_CAP_NR_VCPUS);
    if (ret <= 0)
        /*
         * api.txt states that if KVM_CAP_NR_VCPUS does not exist,
         * assume 4.
         */
        return 4;

    return ret;
}

int kvm_max_cpus(struct kvm *kvm) {
    int ret;

    ret = ioctl(kvm->kvm_fd, KVM_CHECK_EXTENSION, KVM_CAP_MAX_VCPUS);
    if (ret <= 0)
        ret = kvm_recommended_cpus(kvm);

    return ret;
}

int __attribute__((weak)) kvm_get_vm_type(struct kvm *kvm) {
    return KVM_VM_TYPE;
}

int kvm_init(struct vm *vm) {
    int ret;
    struct kvm *kvm = &vm->kvm;

    if (!kvm_arch_cpu_supports_vm()) {
        ERR("Your CPU does not support hardware virtualization");
        ret = -ENOSYS;
        goto err;
    }

    kvm->kvm_fd = open(kvm->cfg.kvm_dev, O_RDWR);

    if (kvm->kvm_fd < 0) {
        if (errno == ENOENT)
            pr_err(
                "'%s' not found. Please make sure your kernel has CONFIG_KVM "
                "enabled and that the KVM modules are loaded.",
                kvm->cfg.kvm_dev);
        else if (errno == ENODEV)
            pr_err(
                "'%s' KVM driver not available.\n  # (If the KVM "
                "module is loaded then 'dmesg' may offer further clues "
                "about the failure.)",
                kvm->cfg.kvm_dev);
        else
            pr_err("Could not open %s: ", kvm->cfg.kvm_dev);

        ret = -errno;
        goto err_free;
    }

    ret = ioctl(kvm->kvm_fd, KVM_GET_API_VERSION, 0);
    if (ret != KVM_API_VERSION) {
        pr_err("KVM_API_VERSION ioctl");
        ret = -errno;
        goto err_sys_fd;
    }

    kvm->vm_fd = ioctl(kvm->kvm_fd, KVM_CREATE_VM, kvm_get_vm_type(kvm));
    if (kvm->vm_fd < 0) {
        pr_err("KVM_CREATE_VM ioctl");
        ret = kvm->vm_fd;
        goto err_sys_fd;
    }

    if (kvm_check_extensions(kvm)) {
        pr_err("A required KVM extension is not supported by OS");
        ret = -ENOSYS;
        goto err_vm_fd;
    }

    kvm_arch_init(kvm);

    INIT_LIST_HEAD(&kvm->mem_banks);
    kvm_init_ram(kvm);

    if (!kvm->cfg.firmware_path) {
        if (!kvm_load_kernel(kvm, kvm->cfg.kernel_path, kvm->cfg.initrd_path, kvm->cfg.real_cmdline))
            die("unable to load kernel %s", kvm->cfg.kernel_path);
    }

    if (kvm->cfg.firmware_path) {
        if (!kvm_load_firmware(kvm, kvm->cfg.firmware_path))
            die("unable to load firmware image %s: %s", kvm->cfg.firmware_path, strerror(errno));
    } else {
        ret = kvm_arch_setup_firmware(kvm);
        if (ret < 0)
            die("kvm_arch_setup_firmware() failed with error %d\n", ret);
    }

    return 0;

err_vm_fd:
    close(kvm->vm_fd);
err_sys_fd:
    close(kvm->kvm_fd);
err_free:
    free(kvm);
err:
    return ret;
}
core_init(kvm_init);

bool kvm_load_kernel(struct kvm *kvm, const char *kernel_filename, const char *initrd_filename,
                     const char *kernel_cmdline) {
    bool ret;
    int fd_kernel = -1, fd_initrd = -1;

    fd_kernel = open(kernel_filename, O_RDONLY);
    if (fd_kernel < 0)
        die("Unable to open kernel %s", kernel_filename);

    if (initrd_filename) {
        fd_initrd = open(initrd_filename, O_RDONLY);
        if (fd_initrd < 0)
            die("Unable to open initrd %s", initrd_filename);
    }

    ret = kvm_arch_load_kernel_image(kvm, fd_kernel, fd_initrd, kernel_cmdline);

    if (initrd_filename)
        close(fd_initrd);
    close(fd_kernel);

    if (!ret)
        die("%s is not a valid kernel image", kernel_filename);
    return ret;
}

void kvm_dump_mem(struct kvm *kvm, unsigned long addr, unsigned long size, int debug_fd) {
    unsigned char *p;
    unsigned long n;

    size &= ~7; /* mod 8 */
    if (!size)
        return;

    p = guest_flat_to_host(kvm, addr);

    for (n = 0; n < size; n += 8) {
        if (!host_ptr_in_ram(kvm, p + n)) {
            dprintf(debug_fd, " 0x%08lx: <unknown>\n", addr + n);
            continue;
        }
        dprintf(debug_fd,
                " 0x%08lx: %02x %02x %02x %02x  %02x %02x %02x %02x\n",
                addr + n,
                p[n + 0],
                p[n + 1],
                p[n + 2],
                p[n + 3],
                p[n + 4],
                p[n + 5],
                p[n + 6],
                p[n + 7]);
    }
}

void kvm_reboot(struct kvm *kvm) {
    /* Check if the guest is running */
    if (!kvm->cpus[0] || kvm->cpus[0]->thread == 0)
        return;

    pthread_kill(kvm->cpus[0]->thread, SIGKVMEXIT);
}

void kvm_continue(struct kvm *kvm) {
    mutex_unlock(&pause_lock);
}

void kvm_pause(struct kvm *kvm) {
    int i, paused_vcpus = 0;

    mutex_lock(&pause_lock);

    /* Check if the guest is running */
    if (!kvm->cpus || !kvm->cpus[0] || kvm->cpus[0]->thread == 0)
        return;

    pause_event = eventfd(0, 0);
    if (pause_event < 0)
        die("Failed creating pause notification event");
    for (i = 0; i < kvm->nrcpus; i++) {
        if (kvm->cpus[i]->is_running && kvm->cpus[i]->paused == 0)
            pthread_kill(kvm->cpus[i]->thread, SIGKVMPAUSE);
        else
            paused_vcpus++;
    }

    while (paused_vcpus < kvm->nrcpus) {
        u64 cur_read;

        if (read(pause_event, &cur_read, sizeof(cur_read)) < 0)
            die("Failed reading pause event");
        paused_vcpus += cur_read;
    }
    close(pause_event);
}

void kvm_notify_paused(void) {
    u64 p = 1;

    if (write(pause_event, &p, sizeof(p)) < 0)
        die("Failed notifying of paused VCPU.");

    mutex_lock(&pause_lock);
    current_kvm_cpu->paused = 0;
    mutex_unlock(&pause_lock);
}
