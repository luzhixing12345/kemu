
#include <clib/clib.h>
#include <kvm/init.h>
#include <kvm/kvm-config.h>
#include <kvm/kvm-cpu.h>
#include <kvm/term.h>
#include <stdio.h>
#include <string.h>
#include <vm/vm.h>

#include "clib/log.h"
#include "memory.h"

int vm_validate_cfg(struct vm_config *config) {
    if (!config->kernel.kernel_path) {
        ERR("kernel path is not set\n");
        return -EINVAL;
    }

    return 0;
}

static int vm_add_disk(struct vm *vm, const char *disk_path) {
    // check if disk_path exists
    if (!path_exist(disk_path)) {
        ERR("disk path %s does not exist", disk_path);
        return -ENOENT;
    }
    // check if disk_path is already added
    for (int i = 0; i < vm->nr_disks; i++) {
        // TODO: check if file is the same
        if (vm->disks[i].disk_path == disk_path) {
            WARNING("disk path %s is already added", disk_path);
            return 0;
        }
    }

    vm->nr_disks += 1;
    vm->disks = realloc(vm->disks, vm->nr_disks * sizeof(struct disk_image));
    if (!vm->disks) {
        ERR("failed to allocate memory for vm->disks");
        return -ENOMEM;
    }
    struct disk_image *new_disk = &vm->disks[vm->nr_disks - 1];
    memset(new_disk, 0, sizeof(struct disk_image));
    new_disk->disk_path = disk_path;
    return 0;
}

int vm_config_init(struct vm *vm) {
    struct vm_config *vm_config = &vm->cfg;        // configuration of the vm
    struct kvm_config *kvm_config = &vm->kvm.cfg;  // configuration of the kvm

    kvm_config->mem_addr = kvm_arch_default_ram_address();

    // CPU
    if (!vm_config->cpu.nrcpus) {
        int nr_online_cpus = sysconf(_SC_NPROCESSORS_ONLN);
        // KVM vCPU number can be greater than the number of online cpus
        // Virtual CPUs do not have to be mapped to physical CPU cores one by one;
        // KVM utilizes the host's CPU time slices to schedule virtual CPUs.
        // see Overcommitment
        if (vm_config->cpu.nrcpus > nr_online_cpus) {
            WARNING(
                "cpu number %u is greater than the number of online cpus %u", vm_config->cpu.nrcpus, nr_online_cpus);
        }
        vm_config->cpu.nrcpus = nr_online_cpus;
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
    if (!vm_config->system.vm_name) {
        static char guest_name[32];
        snprintf(guest_name, sizeof(guest_name), "%s-%d", DEFAULT_GUEST_NAME, getpid());
        vm_config->system.vm_name = guest_name;
        DEBUG("name: %s", vm_config->system.vm_name);
    }

    // Storage
    vm->nr_disks = 0;
    vm->disks = NULL;
    if (vm->cfg.drive.disk_path) {
        if (vm_add_disk(vm, vm->cfg.drive.disk_path) < 0) {
            return -1;
        }
    }

    if (vm->nr_disks == 0) {
        ERR("no disk specified");
        return -1;
    }

    return 0;
}

int vm_rootfs_init(struct vm *vm) {
    static char rootfs_path[64];
    sprintf(rootfs_path, "%s/%s", DEFAULT_ROOTFS_PATH, vm->cfg.system.vm_name);
    vm->cfg.system.rootfs_path = rootfs_path;

    // if DEFAULT_ROOTFS_PATH does not exist, create it
    if (!path_exist(DEFAULT_ROOTFS_PATH)) {
        if (mkdir(DEFAULT_ROOTFS_PATH, 0755) < 0) {
            ERR("failed to create rootfs %s\n", DEFAULT_ROOTFS_PATH);
            return -1;
        }
    }

    // check if the rootfs exists, delete it if it does
    if (path_exist(rootfs_path)) {
        DEBUG("rootfs %s exists, delete it", rootfs_path);
        if (del_dir(rootfs_path) < 0) {
            ERR("failed to delete rootfs %s", rootfs_path);
            return -1;
        }
    }
    // create rootfs
    if (mkdir(rootfs_path, 0755) < 0) {
        ERR("failed to create rootfs %s\n", rootfs_path);
        return -1;
    }
    INFO("create rootfs %s", rootfs_path);
    return 0;
}

int vm_rootfs_exit(struct vm *vm) {
    // delete rootfs
    char *rootfs_path = vm->cfg.system.rootfs_path;
    if (del_dir(rootfs_path) < 0) {
        ERR("failed to delete rootfs %s\n", rootfs_path);
        return -1;
    }
    DEBUG("delete rootfs %s", rootfs_path);
    return 0;
}

int vm_run(struct vm *vm) {
    struct kvm *kvm = &vm->kvm;
    for (int i = 0; i < kvm->nrcpus; i++) {
        if (pthread_create(&kvm->cpus[i]->thread, NULL, kvm_cpu_thread, kvm->cpus[i]) != 0)
            ERR("unable to create KVM VCPU thread");
        DEBUG("vcpu %d created", i);
    }

    /* Only VCPU #0 is going to exit by itself when shutting down */
    if (pthread_join(kvm->cpus[0]->thread, NULL) != 0)
        DIE("unable to join with vcpu 0");

    return kvm_cpu_exit(vm);
}

int vm_init(struct vm *vm) {
    int ret = 0;
    ret = vm_config_init(vm);
    if (ret < 0)
        goto fail;

    ret = vm_rootfs_init(vm);
    if (ret < 0)
        goto fail;

    ret = init_list_init(vm);
fail:
    return ret;
}

int vm_exit(struct vm *vm) {
    init_list_exit(vm);
    vm_rootfs_exit(vm);

    if (vm->nr_disks) {
        free(vm->disks);
    }
    return 0;
}