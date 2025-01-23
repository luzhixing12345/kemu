#include <kvm/init.h>
#include <kvm/kvm.h>
#include <kvm/term.h>
#include <linux/err.h>
#include <simple-clib/logging.h>
#include <simple-clib/xargparse.h>
#include <stdio.h>
#include <vm/vm.h>

#include "kvm/kvm-config.h"
#include "kvm/mutex.h"
#include "memory.h"

#define SYSTEM_OPTION  "\n\n[system options]"
#define STORAGE_OPTION "\n\n[storage options]"
#define NETWORK_OPTION "\n\n[network options]"
#define DEBUG_OPTION   "\n\n[debug options]"

__thread struct kvm_cpu *current_kvm_cpu;
int loglevel = LOGLEVEL_INFO;

void vm_validate_cfg(struct vm_config *config) {
}

int vm_run_init(struct vm *vm) {
    return 0;
}

int vm_run_work() {
    return 0;
}

void vm_run_exit() {
    // init_list_exit();
}

int vm_run(struct vm *vm) {
    int ret = -EFAULT;

    vm_run_init(vm);

    ret = vm_run_work();
    vm_run_exit();

    return ret;
}

int main(int argc, const char **argv) {
    struct vm kemu_vm;
    memset(&kemu_vm, 0, sizeof(struct kvm));
    argparse_option options[] = {
        // basic system boot options
        XBOX_ARG_STR(&kemu_vm.cfg.system.name, NULL, "--name", "guest vm name", " <name>", "name"),
        XBOX_ARG_STR(&kemu_vm.cfg.kernel.kernel_path, NULL, "--kernel", "kernel binary path", " <bzImage>", "kernel"),
        XBOX_ARG_STR(
            &kemu_vm.cfg.kernel.kernel_cmdline, NULL, "--append", "kernel cmdline", " <cmdline>", "kernel-cmdline"),
        XBOX_ARG_STR(&kemu_vm.cfg.drive.disk_path, NULL, "--disk", "disk path", " <disk>", "disk"),
        XBOX_ARG_STR(&kemu_vm.cfg.memory.mem_size_str, "-m", NULL, "memory size", " <memory-size>", "memory"),
        XBOX_ARG_INT(&kemu_vm.cfg.cpu.nrcpus, NULL, "--smp", "cpu number", " <cpus>", "cpu"),
        XBOX_ARG_STRS(
            &kemu_vm.cfg.device.devices_str, NULL, "--device", "device" STORAGE_OPTION, " <device>", "device"),
        // storage options
        XBOX_ARG_STRS(&kemu_vm.cfg.drive.drives_str, NULL, "--drive", "drive", " <driver>", "driver"),
        XBOX_ARG_STR(&kemu_vm.cfg.drive.hda, NULL, "--hda", "harddisk", " <harddisk>", NULL),
        XBOX_ARG_STR(&kemu_vm.cfg.drive.hdb, NULL, "--hdb", "harddisk", " <harddisk>", NULL),
        XBOX_ARG_STRS(&kemu_vm.cfg.drive.cdrom, NULL, "--cdrom", "cdrom" NETWORK_OPTION, " <cdrom>", "cdrom"),
        // network options
        XBOX_ARG_STRS(&kemu_vm.cfg.network.network, NULL, "--net", "network" DEBUG_OPTION, " <network>", "network"),
        // debug options
        XBOX_ARG_STR(&kemu_vm.cfg.debug.log_file, "-D", NULL, "log file", " <log-file>", "log-file"),
        XBOX_ARG_STR(&kemu_vm.cfg.debug.gdb_server, NULL, "--gdb", "gdb server", " tcp::<gdb-port>", "gdb-server"),
        XBOX_ARG_BOOLEAN(&kemu_vm.cfg.debug.stop, "-S", NULL, "stop after boot", NULL, "stop"),
        XBOX_ARG_BOOLEAN(&kemu_vm.cfg.debug.enable_gdb, "-s", NULL, "enable gdb server", NULL, "gdb"),
        XBOX_ARG_BOOLEAN(NULL, "-h", "--help", "show help information", NULL, "help"),
        XBOX_ARG_BOOLEAN(NULL, "-v", "--version", "show version", NULL, "version"),
        XBOX_ARG_END()};

    XBOX_argparse parser;
    XBOX_argparse_init(&parser, options, 0);
    XBOX_argparse_describe(
        &parser,
        "kemu",
        "\na clean, from-scratch, lightweight full system hypervisor for hosting KVM guests" SYSTEM_OPTION,
        "\nGithub: https://github.com/luzhixing12345/kemu\n");
    XBOX_argparse_parse(&parser, argc, argv);
    if (XBOX_ismatch(&parser, "help")) {
        XBOX_argparse_info(&parser);
        XBOX_free_argparse(&parser);
        return 0;
    }
    if (XBOX_ismatch(&parser, "version")) {
        printf("kemu version: %s\n", CONFIG_VERSION);
        XBOX_free_argparse(&parser);
        return 0;
    }
    vm_validate_cfg(&kemu_vm.cfg);
    vm_init(&kemu_vm);
    vm_run(&kemu_vm);
    XBOX_free_argparse(&parser);
    return 0;
}