#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "kvm/kvm.h"
#include "kvm/util.h"

#define BIOS_SELECTOR 0xf000
#define BIOS_IP       0xfff0
#define BIOS_SP       0x8000

bool kvm_load_firmware(struct kvm *kvm, const char *firmware_filename) {
    struct stat st;
    void *p;
    int fd;
    int nr;

    fd = open(firmware_filename, O_RDONLY);
    if (fd < 0)
        return false;

    if (fstat(fd, &st))
        return false;

    if (st.st_size > MB_FIRMWARE_BIOS_SIZE)
        die("firmware image %s is too big to fit in memory (%Lu KB).\n", firmware_filename, (u64)(st.st_size / 1024));

    p = guest_flat_to_host(kvm, MB_FIRMWARE_BIOS_BEGIN);

    while ((nr = read(fd, p, st.st_size)) > 0) p += nr;

    kvm->arch.boot_selector = BIOS_SELECTOR;
    kvm->arch.boot_ip = BIOS_IP;
    kvm->arch.boot_sp = BIOS_SP;

    return true;
}
