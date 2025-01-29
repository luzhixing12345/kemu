#include <clib/clib.h>
#include <kvm/init.h>
#include <kvm/kvm.h>
#include <kvm/term.h>
#include <linux/err.h>
#include <stdio.h>
#include <vm/vm.h>

#include "kvm/kvm-config.h"
#include "kvm/kvm-cpu.h"
#include "kvm/mutex.h"
#include "memory.h"

#define SYSTEM_OPTION  "\n\n[system options]"
#define STORAGE_OPTION "\n\n[storage options]"
#define NETWORK_OPTION "\n\n[network options]"
#define DEBUG_OPTION   "\n\n[debug options]"

__thread struct kvm_cpu *current_kvm_cpu;
int loglevel = LOGLEVEL_INFO;

int main(int argc, const char **argv) {
    struct vm kemu_vm;
    memset(&kemu_vm, 0, sizeof(struct kvm));
    argparse_option options[] = {
        // basic system boot options
        ARG_STR(&kemu_vm.cfg.system.vm_name, NULL, "--name", "guest vm name", " <name>", "name"),
        ARG_STR(&kemu_vm.cfg.kernel.kernel_path, NULL, "--kernel", "kernel binary path", " <bzImage>", "kernel"),
        ARG_STR(
            &kemu_vm.cfg.kernel.kernel_cmdline, NULL, "--append", "kernel cmdline", " <cmdline>", "kernel-cmdline"),
        ARG_STR(&kemu_vm.cfg.drive.disk_path, NULL, "--disk", "disk path", " <disk>", "disk"),
        ARG_STR(&kemu_vm.cfg.memory.mem_size_str, "-m", NULL, "memory size", " <memory-size>", "memory"),
        ARG_INT(&kemu_vm.cfg.cpu.nrcpus, NULL, "--smp", "cpu number", " <cpus>", "cpu"),
        ARG_STRS(
            &kemu_vm.cfg.device.devices_str, NULL, "--device", "device" STORAGE_OPTION, " <device>", "device"),
        // storage options
        ARG_STRS(&kemu_vm.cfg.drive.drives_str, NULL, "--drive", "drive", " <driver>", "driver"),
        ARG_STR(&kemu_vm.cfg.drive.hda, NULL, "--hda", "harddisk", " <harddisk>", NULL),
        ARG_STR(&kemu_vm.cfg.drive.hdb, NULL, "--hdb", "harddisk", " <harddisk>", NULL),
        ARG_STRS(&kemu_vm.cfg.drive.cdrom, NULL, "--cdrom", "cdrom" NETWORK_OPTION, " <cdrom>", "cdrom"),
        // network options
        ARG_STRS(&kemu_vm.cfg.network.network, NULL, "--net", "network" DEBUG_OPTION, " <network>", "network"),
        // debug options
        ARG_STR(&kemu_vm.cfg.debug.log_file, "-D", NULL, "log file", " <log-file>", "log-file"),
        ARG_STR(&kemu_vm.cfg.debug.gdb_server, NULL, "--gdb", "gdb server", " tcp::<gdb-port>", "gdb-server"),
        ARG_BOOLEAN(&kemu_vm.cfg.debug.stop, "-S", NULL, "stop after boot", NULL, "stop"),
        ARG_BOOLEAN(&kemu_vm.cfg.debug.enable_gdb, "-s", NULL, "enable gdb server", NULL, "gdb"),
        ARG_BOOLEAN(NULL, "-h", "--help", "show help information", NULL, "help"),
        ARG_BOOLEAN(NULL, "-v", "--version", "show version", NULL, "version"),
        ARG_END()};

    argparse parser;
    argparse_init(&parser, options, 0);
    argparse_describe(
        &parser,
        "kemu",
        "\na clean, from-scratch, lightweight full system hypervisor for hosting KVM guests" SYSTEM_OPTION,
        "\nGithub: https://github.com/luzhixing12345/kemu\n");
    argparse_parse(&parser, argc, argv);
    if (arg_ismatch(&parser, "help")) {
        argparse_info(&parser);
        free_argparse(&parser);
        return 0;
    }
    if (arg_ismatch(&parser, "version")) {
        printf("kemu version: %s\n", CONFIG_VERSION);
        free_argparse(&parser);
        return 0;
    }

    int ret = 0;
    ret = vm_validate_cfg(&kemu_vm.cfg);
    if (ret < 0)
        goto end;

    ret = vm_init(&kemu_vm);
    if (ret < 0)
        goto end;
    
    vm_run(&kemu_vm);
    vm_exit(&kemu_vm);

end:
    free_argparse(&parser);
    return 0;
}