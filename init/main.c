#include <kvm/init.h>
#include <kvm/kvm.h>
#include <kvm/term.h>
#include <linux/err.h>
#include <simple-clib/logging.h>
#include <simple-clib/xargparse.h>
#include <stdio.h>

#include "kemu.h"
#include "kvm/kvm-config.h"
#include "kvm/mutex.h"
#include "memory.h"

#define SYSTEM_OPTION  "\n\n[system options]"
#define STORAGE_OPTION "\n\n[storage options]"
#define NETWORK_OPTION "\n\n[network options]"
#define DEBUG_OPTION   "\n\n[debug options]"

__thread struct kvm_cpu *current_kvm_cpu;
int loglevel = LOGLEVEL_INFO;
struct kemu kemu;

void kemu_validate_cfg(struct kemu_config *config) {
}

int kemu_run_init(struct kemu_config *config) {
    struct kvm *kvm = kvm_new();
    if (IS_ERR(kvm)) {
        ERR("kvm_new failed\n");
        return -1;
    }
    kvm->cfg.ram_addr = kvm__arch_default_ram_address();

    if (!config->cpu_num) {
        kvm->cfg.nrcpus = DEFAULT_NRCPUS;
    } else {
        unsigned int nr_online_cpus = sysconf(_SC_NPROCESSORS_ONLN);
        // KVM vCPU number can be greater than the number of online cpus
        // Virtual CPUs do not have to be mapped to physical CPU cores one by one;
        // KVM utilizes the host's CPU time slices to schedule virtual CPUs.
        // see Overcommitment
        if (config->cpu_num > nr_online_cpus) {
            WARNING("cpu number %u is greater than the number of online cpus %u\n", config->cpu_num, nr_online_cpus);
        }
        kvm->cfg.nrcpus = config->cpu_num;
    }

    if (!config->ram_size_str) {
        kvm->cfg.ram_size = get_ram_size(kvm->cfg.nrcpus);
    } else {
        kvm->cfg.ram_size = calculate_ram_size(config->ram_size_str);
        if (!kvm->cfg.ram_size) {
            goto init_fail;
        }
    }

    kvm->cfg.dev = DEFAULT_KVM_DEV;
    kvm->cfg.console = DEFAULT_CONSOLE;
    
    if (!strncmp(kvm->cfg.console, "virtio", 6))
        kvm->cfg.active_console = CONSOLE_VIRTIO;
    else if (!strncmp(kvm->cfg.console, "serial", 6))
        kvm->cfg.active_console = CONSOLE_8250;
    else if (!strncmp(kvm->cfg.console, "hv", 2))
        kvm->cfg.active_console = CONSOLE_HV;
    else
        WARNING("No console!");

    
    // init_list_init();

init_fail:
    free(kvm);
    return -1;
}

int kemu_run_work() {
    return 0;
}

void kemu_run_exit() {
    free(kemu.kvm);
    // init_list_exit();
}

int kemu_run(struct kemu_config *config) {
    int ret = -EFAULT;

    kemu_run_init(config);

    ret = kemu_run_work();
    kemu_run_exit();

    return ret;
}

int main(int argc, const char **argv) {
    struct kemu_config config;
    memset(&config, 0, sizeof(struct kemu_config));
    argparse_option options[] = {
        // basic system boot options
        XBOX_ARG_STR(&config.name, NULL, "--name", "guest vm name", " <name>", "name"),
        XBOX_ARG_STR(&config.kernel_path, NULL, "--kernel", "kernel binary path", " <bzImage>", "kernel"),
        XBOX_ARG_STR(&config.kernel_cmdline, NULL, "--append", "kernel cmdline", " <cmdline>", "kernel-cmdline"),
        XBOX_ARG_STR(&config.disk_path, NULL, "--disk", "disk path", " <disk>", "disk"),
        XBOX_ARG_STR(&config.ram_size_str, "-m", NULL, "memory size", " <memory-size>", "memory"),
        XBOX_ARG_INT(&config.cpu_num, NULL, "--smp", "cpu number", " <cpus>", "cpu"),
        XBOX_ARG_STRS(&config.devices, NULL, "--device", "device" STORAGE_OPTION, " <device>", "device"),
        // storage options
        XBOX_ARG_STRS(&config.drivers, NULL, "--drive", "drive", " <driver>", "driver"),
        XBOX_ARG_STR(&config.hda, NULL, "--hda", "harddisk", " <harddisk>", NULL),
        XBOX_ARG_STR(&config.hdb, NULL, "--hdb", "harddisk", " <harddisk>", NULL),
        XBOX_ARG_STRS(&config.cdroms, NULL, "--cdrom", "cdrom" NETWORK_OPTION, " <cdrom>", "cdrom"),
        // network options
        XBOX_ARG_STRS(&config.network, NULL, "--net", "network" DEBUG_OPTION, " <network>", "network"),
        // debug options
        XBOX_ARG_STR(&config.serial, NULL, "--serial", "serial", " <serial>", "serial"),
        XBOX_ARG_STR(&config.log_file, "-D", NULL, "log file", " <log-file>", "log-file"),
        XBOX_ARG_STR(&config.gdb_server, NULL, "--gdb", "gdb server", " tcp::<gdb-port>", "gdb-server"),
        XBOX_ARG_BOOLEAN(&config.stop, "-S", NULL, "stop after boot", NULL, "stop"),
        XBOX_ARG_BOOLEAN(&config.enable_gdb, "-s", NULL, "enable gdb server", NULL, "gdb"),
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
    kemu_validate_cfg(&config);
    kemu_run(&config);
    XBOX_free_argparse(&parser);
    return 0;
}