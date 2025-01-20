
#pragma once

#include <kvm/mutex.h>
#include <linux/types.h>

struct kemu_config {
    u64 ram_addr;  // Guest memory physical base address, in bytes
    u64 ram_size;  // Guest memory size, in bytes
    // basic system boot options
    const char *name;
    const char *kernel_cmdline;
    const char *kernel_path;
    const char *disk_path;
    const char *vmlinux_filename;
    const char *initrd_filename;
    const char *firmware_filename;
    const char *flash_filename;
    const char *console;
    const char *dev;
    const char *host_ip;
    const char *guest_ip;
    const char *guest_mac;
    const char *host_mac;
    const char *script;
    const char *guest_name;
    const char *sandbox;
    const char *hugetlbfs_path;
    const char *custom_rootfs_name;
    const char *real_cmdline;
    int memory_size;
    int cpu_num;
    char **devices;

    // network options
    char **network;

    // storage options
    char **drivers;
    const char *hda;
    const char *hdb;
    char **cdroms;

    // debug options
    const char *serial;
    const char *log_file;
    const char *gdb_server;
    int stop;  // -S
    int enable_gdb;
};

struct kemu_struct {
    int kvm_fd;  // open("/dev/kvm")
    int vm_fd;   // ioctl(KVM_CREATE_VM)
    struct mutex mem_lock;
    struct kemu_config cfg;
};

int kemu_run(struct kemu_config *config);