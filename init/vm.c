
#include <clib/clib.h>
#include <kvm/term.h>
#include <stdio.h>
#include <string.h>
#include <vm/vm.h>

#include "clib/file.h"
#include "kvm/kvm-config.h"
#include "memory.h"

static void get_kernel_real_cmdline(struct vm *vm) {
    static char real_cmdline[2048];
    memset(real_cmdline, 0, sizeof(real_cmdline));
    struct kvm *kvm = &vm->kvm;

    switch (kvm->cfg.active_console) {
        case CONSOLE_HV:
            /* Fallthrough */
        case CONSOLE_VIRTIO:
            strcat(real_cmdline, " console=hvc0");
            break;
        case CONSOLE_8250:
            strcat(real_cmdline, " console=ttyS0");
            break;
    }

    if (!vm->cfg.kernel.kernel_cmdline || !strstr(vm->cfg.kernel.kernel_cmdline, "root=")) {
        strcat(real_cmdline, " root=/dev/vda rw ");
    }

    if (vm->cfg.kernel.kernel_cmdline) {
        strcat(real_cmdline, " ");
        strcat(real_cmdline, vm->cfg.kernel.kernel_cmdline);
    }

    kvm->cfg.real_cmdline = real_cmdline;
    INFO("cmdline: %s\n", real_cmdline);
}

int vm_config_init(struct vm *vm) {
    struct vm_config *vm_config = &vm->cfg;        // configuration of the vm
    struct kvm_config *kvm_config = &vm->kvm.cfg;  // configuration of the kvm

    // kernel
    if (!vm_config->kernel.kernel_path) {
        ERR("kernel path is not set\n");
        return -EINVAL;
    }
    get_kernel_real_cmdline(vm);
    kvm_config->kernel_path = vm_config->kernel.kernel_path;
    INFO("kernel: %s\n", vm_config->kernel.kernel_path);

    kvm_config->mem_addr = kvm_arch_default_ram_address();

    // CPU
    if (!vm_config->cpu.nrcpus) {
        vm_config->cpu.nrcpus = DEFAULT_NRCPUS;
    } else {
        int nr_online_cpus = sysconf(_SC_NPROCESSORS_ONLN);
        // KVM vCPU number can be greater than the number of online cpus
        // Virtual CPUs do not have to be mapped to physical CPU cores one by one;
        // KVM utilizes the host's CPU time slices to schedule virtual CPUs.
        // see Overcommitment
        if (vm_config->cpu.nrcpus > nr_online_cpus) {
            WARNING(
                "cpu number %u is greater than the number of online cpus %u\n", vm_config->cpu.nrcpus, nr_online_cpus);
        }
    }
    kvm_config->nrcpus = vm_config->cpu.nrcpus;

    // Memory
    if (!vm_config->memory.mem_size_str) {
        vm_config->memory.mem_size = get_ram_size(vm_config->cpu.nrcpus);
    } else {
        vm_config->memory.mem_size = calculate_ram_size(vm_config->memory.mem_size_str);
        if (!vm_config->memory.mem_size) {
            return -1;
        }
    }
    kvm_config->mem_size = vm_config->memory.mem_size;

    vm_config->device.kvm_dev = DEFAULT_KVM_DEV;
    kvm_config->kvm_dev = vm_config->device.kvm_dev;

    vm_config->device.console = DEFAULT_CONSOLE;
    kvm_config->console = vm_config->device.console;

    if (!strncmp(vm_config->device.console, "virtio", 6)) {
        vm_config->device.active_console = CONSOLE_VIRTIO;
    } else if (!strncmp(vm_config->device.console, "serial", 6)) {
        vm_config->device.active_console = CONSOLE_8250;
    } else if (!strncmp(vm_config->device.console, "hv", 2)) {
        vm_config->device.active_console = CONSOLE_HV;
    } else {
        WARNING("No console!");
    }
    kvm_config->active_console = vm_config->device.active_console;

    if (!vm_config->network.host_ip) {
        vm_config->network.host_ip = DEFAULT_HOST_ADDR;
    }
    if (!vm_config->network.guest_ip) {
        vm_config->network.guest_ip = DEFAULT_GUEST_ADDR;
    }
    if (!vm_config->network.host_mac) {
        vm_config->network.host_mac = DEFAULT_HOST_MAC;
    }
    if (!vm_config->network.guest_mac) {
        vm_config->network.guest_mac = DEFAULT_GUEST_MAC;
    }
    if (!vm_config->system.name) {
        static char guest_name[32];
        snprintf(guest_name, sizeof(guest_name), "%s-%d", DEFAULT_GUEST_NAME, getpid());
        vm_config->system.name = guest_name;
        kvm_config->name = vm_config->system.name;
        DEBUG("name: %s", vm_config->system.name);
    }

    return 0;
}

int vm_rootfs_init(struct vm *vm) {
    char rootfs_path[32];
    snprintf(rootfs_path, sizeof(rootfs_path), "%s/%s", DEFAULT_ROOTFS_PATH, KEMU);
    // check if the rootfs exists, delete it if it does
    if (path_exist(rootfs_path)) {
        DEBUG("rootfs %s exists, delete it\n", rootfs_path);
        if (del_dir(rootfs_path) < 0) {
            ERR("failed to delete rootfs %s\n", rootfs_path);
            return -1;
        }
    }
    // create rootfs
    if (mkdir(rootfs_path, 0755) < 0) {
        ERR("failed to create rootfs %s\n", rootfs_path);
        return -1;
    }

    return 0;
}

int vm_init(struct vm *vm) {
    int ret = 0;
    vm_config_init(vm);
    vm_rootfs_init(vm);
    init_list_init(vm);
    return ret;
}