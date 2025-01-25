
#pragma once

#include <clib/clib.h>
#include <kvm/kvm.h>

struct vm_config {
    struct kvm_config_arch arch;
    struct disk_image_params disk_image[MAX_DISK_IMAGES];
    struct vfio_device_params *vfio_devices;
    struct {
        const char *vm_name;
        char *rootfs_path;         // /tmp/kemu/{vm_name}
        char *rootfs_socket_path;  // /tmp/kemu/{vm_name}/ipc.sock
    } system;
    struct {
        u64 mem_addr; /* Guest memory physical base address, in bytes */
        u64 mem_size; /* Guest memory size, in bytes */
        char *mem_size_str;
    } memory;

    struct {
        const char *console;
        int active_console;
        const char *kvm_dev;
        char **devices_str;
    } device;
    u8 num_net_devices;
    u8 num_vfio_devices;
    u64 vsock_cid;
    bool virtio_rng;
    bool nodefaults;
    int debug_iodelay;
    struct {
        const char *kernel_cmdline;
        const char *kernel_path;
        const char *vmlinux_filename;
        const char *initrd_filename;
        const char *firmware_filename;
    } kernel;
    struct {
        int nrcpus;
    } cpu;
    struct {
        const char *disk_path;
        const char **drives_str;
        const char *hda;
        const char *hdb;
        const char *cdrom;
    } drive;
    const char *flash_filename;
    struct {
        const char *network;
        const char *host_ip;
        const char *host_mac;
        const char *guest_ip;
        const char *guest_mac;
    } network;
    struct {
        const char *log_file;
        const char *gdb_server;
        bool stop;
        bool enable_gdb;
    } debug;
    const char *script;
    const char *sandbox;
    const char *hugetlbfs_path;
    const char *custom_rootfs_name;
    const char *real_cmdline;
    struct virtio_net_params *net_params;
    bool single_step;
    bool vnc;
    bool gtk;
    bool sdl;
    bool balloon;
    bool using_rootfs;
    bool custom_rootfs;
    bool no_net;
    bool no_dhcp;
    bool ioport_debug;
    bool mmio_debug;
    int virtio_transport;
};

struct vm {
    struct kvm kvm;
    struct vm_config cfg;
};

int vm_init(struct vm *vm);
int vm_exit(struct vm *vm);