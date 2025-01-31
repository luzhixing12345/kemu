
#include <clib/clib.h>
#include <kvm/kvm-config.h>
#include <kvm/kvm-cpu.h>
#include <kvm/term.h>
#include <kvm/util-init.h>
#include <stdio.h>
#include <string.h>
#include <vm/vm.h>

#include "clib/log.h"
#include "memory.h"

int vm_validate_cfg(struct kvm_config *config) {
    if (!config->kernel_filename) {
        ERR("kernel path is not set\n");
        return -EINVAL;
    }

    return 0;
}

static int vm_add_disk(struct kvm *kvm, const char *disk_path) {
    // check if disk_path exists
    if (!path_exist(disk_path)) {
        ERR("disk path %s does not exist", disk_path);
        return -ENOENT;
    }
    // check if disk_path is already added
    for (int i = 0; i < kvm->nr_disks; i++) {
        // TODO: check if file is the same
        if (kvm->disks[i].disk_path == disk_path) {
            WARNING("disk path %s is already added", disk_path);
            return 0;
        }
    }

    kvm->nr_disks += 1;
    kvm->disks = realloc(kvm->disks, kvm->nr_disks * sizeof(struct disk_image));
    if (!kvm->disks) {
        ERR("failed to allocate memory for vm->disks");
        return -ENOMEM;
    }
    struct disk_image *new_disk = &kvm->disks[kvm->nr_disks - 1];
    memset(new_disk, 0, sizeof(struct disk_image));
    new_disk->disk_path = disk_path;
    return 0;
}

void get_kernel_real_cmdline(struct kvm *kvm) {
    static char real_cmdline[2048];
    memset(real_cmdline, 0, sizeof(real_cmdline));
    kvm__arch_set_cmdline(real_cmdline, false);

    switch (kvm->cfg.active_console) {
        case CONSOLE_HV:
            /* Fallthrough */
        case CONSOLE_VIRTIO:
            strcat(real_cmdline, " console=hvc0");
            break;
        case CONSOLE_8250:
            strcat(real_cmdline, " console=ttyS0");
            break;
        default:
            WARNING("Unknown console type: %d", kvm->cfg.active_console);
            break;
    }

    if (!kvm->cfg.kernel_cmdline || !strstr(kvm->cfg.kernel_cmdline, "root=")) {
        strcat(real_cmdline, " root=/dev/vda rw ");
    }

    if (kvm->cfg.kernel_cmdline) {
        strcat(real_cmdline, " ");
        strcat(real_cmdline, kvm->cfg.kernel_cmdline);
    }

    kvm->cfg.real_cmdline = real_cmdline;
    INFO("real kernel cmdline: %s", real_cmdline);
}

int vm_config_init(struct kvm *kvm) {
    // struct vm_config *vm_config = &vm->cfg;        // configuration of the vm
    struct kvm_config *config = &kvm->cfg;  // configuration of the kvm

    config->ram_addr = kvm__arch_default_ram_address();

    // CPU
    if (!config->nrcpus) {
        int nr_online_cpus = sysconf(_SC_NPROCESSORS_ONLN);
        // KVM vCPU number can be greater than the number of online cpus
        // Virtual CPUs do not have to be mapped to physical CPU cores one by one;
        // KVM utilizes the host's CPU time slices to schedule virtual CPUs.
        // see Overcommitment
        if (config->nrcpus > nr_online_cpus) {
            WARNING("cpu number %u is greater than the number of online cpus %u", config->nrcpus, nr_online_cpus);
        }
        config->nrcpus = nr_online_cpus;
    }

    // if (!kvm->cfg.kernel_filename && !kvm->cfg.firmware_filename) {
    // 	kvm->cfg.kernel_filename = find_kernel();

    // 	if (!kvm->cfg.kernel_filename) {
    // 		kernel_usage_with_options();
    // 		return ERR_PTR(-EINVAL);
    // 	}
    // }

    // if (kvm->cfg.kernel_filename) {
    // 	kvm->cfg.vmlinux_filename = find_vmlinux();
    // 	kvm->vmlinux = kvm->cfg.vmlinux_filename;
    // }

    // if (kvm->cfg.nrcpus == 0)
    // 	kvm->cfg.nrcpus = nr_online_cpus;

    if (!kvm->cfg.ram_size)
        kvm->cfg.ram_size = get_ram_size(kvm->cfg.nrcpus);

    if (!kvm->cfg.dev)
        kvm->cfg.dev = DEFAULT_KVM_DEV;

    if (!kvm->cfg.console)
        kvm->cfg.console = DEFAULT_CONSOLE;

    if (!strncmp(kvm->cfg.console, "virtio", 6))
        kvm->cfg.active_console = CONSOLE_VIRTIO;
    else if (!strncmp(kvm->cfg.console, "serial", 6))
        kvm->cfg.active_console = CONSOLE_8250;
    else if (!strncmp(kvm->cfg.console, "hv", 2))
        kvm->cfg.active_console = CONSOLE_HV;
    else
        pr_warning("No console!");

    if (!kvm->cfg.host_ip)
        kvm->cfg.host_ip = DEFAULT_HOST_ADDR;

    if (!kvm->cfg.guest_ip)
        kvm->cfg.guest_ip = DEFAULT_GUEST_ADDR;

    if (!kvm->cfg.guest_mac)
        kvm->cfg.guest_mac = DEFAULT_GUEST_MAC;

    if (!kvm->cfg.host_mac)
        kvm->cfg.host_mac = DEFAULT_HOST_MAC;

    if (!kvm->cfg.script)
        kvm->cfg.script = DEFAULT_SCRIPT;

    if (!kvm->cfg.network)
        kvm->cfg.network = DEFAULT_NETWORK;

    if (!kvm->cfg.guest_name) {
        if (kvm->cfg.custom_rootfs) {
            kvm->cfg.guest_name = kvm->cfg.custom_rootfs_name;
        } else {
            static char default_name[20];
            sprintf(default_name, "guest-%u", getpid());
            kvm->cfg.guest_name = default_name;
        }
    }

    // if (!kvm->cfg.nodefaults &&
    //     !kvm->cfg.using_rootfs &&
    //     !kvm->cfg.disk_image[0].filename &&
    //     !kvm->cfg.initrd_filename) {
    // 	char tmp[PATH_MAX];

    // 	kvm_setup_create_new(kvm->cfg.custom_rootfs_name);
    // 	kvm_setup_resolv(kvm->cfg.custom_rootfs_name);

    // 	snprintf(tmp, PATH_MAX, "%s%s", kvm__get_dir(), "default");
    // 	if (virtio_9p__register(kvm, tmp, "/dev/root") < 0)
    // 		die("Unable to initialize virtio 9p");
    // 	if (virtio_9p__register(kvm, "/", "hostfs") < 0)
    // 		die("Unable to initialize virtio 9p");
    // 	kvm->cfg.using_rootfs = kvm->cfg.custom_rootfs = 1;
    // }

    // if (kvm->cfg.custom_rootfs) {
    // 	kvm_run_set_sandbox(kvm);
    // 	if (kvm_setup_guest_init(kvm->cfg.custom_rootfs_name))
    // 		die("Failed to setup init for guest.");
    // }

    if (kvm->cfg.nodefaults)
        kvm->cfg.real_cmdline = kvm->cfg.kernel_cmdline;
    else
        get_kernel_real_cmdline(kvm);

    // if (kvm->cfg.kernel_filename) {
    // 	pr_info("# %s run -k %s -m %Lu -c %d --name %s", KVM_BINARY_NAME,
    // 		kvm->cfg.kernel_filename,
    // 		(unsigned long long)kvm->cfg.ram_size >> MB_SHIFT,
    // 		kvm->cfg.nrcpus, kvm->cfg.guest_name);
    // } else if (kvm->cfg.firmware_filename) {
    // 	pr_info("# %s run --firmware %s -m %Lu -c %d --name %s", KVM_BINARY_NAME,
    // 		kvm->cfg.firmware_filename,
    // 		(unsigned long long)kvm->cfg.ram_size >> MB_SHIFT,
    // 		kvm->cfg.nrcpus, kvm->cfg.guest_name);
    // }

    kvm->nr_disks = 0;
    kvm->disks = NULL;
    if (kvm->cfg.disk_path) {
        if (vm_add_disk(kvm, kvm->cfg.disk_path) < 0) {
            return -1;
        }
    }

    if (kvm->nr_disks == 0) {
        ERR("no disk specified");
        return -1;
    }

    return 0;
}

int vm_rootfs_init(struct kvm *kvm) {
    static char rootfs_path[64];
    sprintf(rootfs_path, "%s/%s", DEFAULT_ROOTFS_PATH, kvm->cfg.guest_name);
    kvm->cfg.rootfs_path = rootfs_path;

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

int vm_rootfs_exit(struct kvm *kvm) {
    // delete rootfs
    char *rootfs_path = kvm->cfg.rootfs_path;
    if (del_dir(rootfs_path) < 0) {
        ERR("failed to delete rootfs %s\n", rootfs_path);
        return -1;
    }
    DEBUG("delete rootfs %s", rootfs_path);
    return 0;
}

int vm_run(struct kvm *kvm) {
    for (int i = 0; i < kvm->nrcpus; i++) {
        if (pthread_create(&kvm->cpus[i]->thread, NULL, kvm_cpu_thread, kvm->cpus[i]) != 0)
            ERR("unable to create KVM VCPU thread");
        DEBUG("vcpu %d created", i);
    }

    /* Only VCPU #0 is going to exit by itself when shutting down */
    if (pthread_join(kvm->cpus[0]->thread, NULL) != 0)
        DIE("unable to join with vcpu 0");

    return kvm_cpu__exit(kvm);
}

int vm_init(struct kvm *kvm) {
    int ret = 0;
    ret = vm_config_init(kvm);
    if (ret < 0)
        goto fail;

    ret = vm_rootfs_init(kvm);
    if (ret < 0)
        goto fail;

    ret = init_list__init(kvm);
fail:
    return ret;
}

int vm_exit(struct kvm *vm) {
    init_list__exit(vm);
    vm_rootfs_exit(vm);

    if (vm->nr_disks) {
        free(vm->disks);
    }
    return 0;
}