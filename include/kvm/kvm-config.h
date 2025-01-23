#ifndef KVM_CONFIG_H_
#define KVM_CONFIG_H_

#include <linux/sizes.h>

#include "kvm/disk-image.h"
#include "kvm/kvm-config-arch.h"
#include "kvm/vfio.h"

#define DEFAULT_KVM_DEV          "/dev/kvm"
#define DEFAULT_CONSOLE          "serial"
#define DEFAULT_NETWORK          "user"
#define DEFAULT_HOST_ADDR        "192.168.33.1"
#define DEFAULT_GUEST_ADDR       "192.168.33.15"
#define DEFAULT_GUEST_MAC        "02:15:15:15:15:15"
#define DEFAULT_GUEST_NAME       "guest-vm"
#define DEFAULT_HOST_MAC         "02:01:01:01:01:01"
#define DEFAULT_SCRIPT           "none"
#define DEFAULT_SANDBOX_FILENAME "guest/sandbox.sh"
#define DEFAULT_NRCPUS           1

#define MIN_RAM_SIZE             SZ_64M

struct kvm_config {
    struct kvm_config_arch arch;
    struct disk_image_params disk_image[MAX_DISK_IMAGES];
    struct vfio_device_params *vfio_devices;
    const char *name;
    const char *kvm_dev;
    const char *console;
    int active_console;
    const char *kernel_path;
    const char *firmware_path;
    const char *initrd_path;
    const char *real_cmdline;
    int nrcpus;
    u64 mem_addr; /* Guest memory physical base address, in bytes */
    u64 mem_size; /* Guest memory size, in bytes */
    u8 num_net_devices;
    u8 num_vfio_devices;
    u64 vsock_cid;
    bool virtio_rng;
    bool nodefaults;
    int debug_iodelay;
    const char *script;
    const char *sandbox;
    const char *hugetlbfs_path;
    const char *custom_rootfs_name;
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

#endif
