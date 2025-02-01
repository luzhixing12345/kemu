#include <clib/clib.h>
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

int main(int argc, const char **argv) {
    struct kvm kemu_vm;
    memset(&kemu_vm, 0, sizeof(struct kvm));
    argparse_option options[] = {
        // basic system boot options
        ARG_STR(&kemu_vm.cfg.guest_name, NULL, "--name", "guest vm name", " <name>", "name"),
        ARG_STR(&kemu_vm.cfg.kernel_path, NULL, "--kernel", "kernel binary path", " <bzImage>", "kernel"),
        ARG_STR(&kemu_vm.cfg.kernel_cmdline, NULL, "--append", "kernel cmdline", " <cmdline>", NULL),
        ARG_STR(&kemu_vm.cfg.ram_size, "-m", NULL, "memory size", " <memory-size>", "memory"),
        ARG_INT(&kemu_vm.cfg.nrcpus, NULL, "--smp", "cpu number", " <cpus>", "cpu"),
        // storage options
        ARG_STR(&kemu_vm.cfg.disk_path, NULL, "--disk", "disk path", " <disk>", "disk"),
        // network options
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
    // ret = vm_validate_cfg(&kemu_vm.cfg);
    if (ret < 0)
        goto end;

    ret = vm_init(&kemu_vm);
    if (ret < 0)
        goto end;

    vm_run(&kemu_vm);
    vm_exit(&kemu_vm);

    return ret;

end:
    free_argparse(&parser);
    return 0;
}