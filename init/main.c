#include <kvm/init.h>
#include <kvm/kvm.h>
#include <kvm/term.h>
#include <linux/err.h>
#include <simple-clib/logging.h>
#include <simple-clib/xargparse.h>
#include <stdio.h>

#include "kvm/kvm-config.h"
#include "kvm/mutex.h"
#include "memory.h"

#define SYSTEM_OPTION  "\n\n[system options]"
#define STORAGE_OPTION "\n\n[storage options]"
#define NETWORK_OPTION "\n\n[network options]"
#define DEBUG_OPTION   "\n\n[debug options]"

__thread struct kvm_cpu *current_kvm_cpu;
int loglevel = LOGLEVEL_INFO;
struct kvm kvm;

void kemu_validate_cfg(struct kvm_config *config) {
}

int kemu_run_init(struct kvm_config *config) {
    config->memory.mem_addr = kvm__arch_default_ram_address();

    if (!config->cpu.nrcpus) {
        config->cpu.nrcpus = DEFAULT_NRCPUS;
    } else {
        int nr_online_cpus = sysconf(_SC_NPROCESSORS_ONLN);
        // KVM vCPU number can be greater than the number of online cpus
        // Virtual CPUs do not have to be mapped to physical CPU cores one by one;
        // KVM utilizes the host's CPU time slices to schedule virtual CPUs.
        // see Overcommitment
        if (config->cpu.nrcpus > nr_online_cpus) {
            WARNING("cpu number %u is greater than the number of online cpus %u\n", config->cpu.nrcpus, nr_online_cpus);
        }
    }

    if (!config->memory.mem_size_str) {
        config->memory.mem_size = get_ram_size(config->cpu.nrcpus);
    } else {
        config->memory.mem_size = calculate_ram_size(config->memory.mem_size_str);
        if (!config->memory.mem_size) {
            return -1;
        }
    }

    config->device.kvm_dev = DEFAULT_KVM_DEV;
    config->device.console = DEFAULT_CONSOLE;
    if (!strncmp(config->device.console, "virtio", 6)) {
        config->device.active_console = CONSOLE_VIRTIO;
    } else if (!strncmp(config->device.console, "serial", 6)) {
        config->device.active_console = CONSOLE_8250;
    } else if (!strncmp(config->device.console, "hv", 2)) {
        config->device.active_console = CONSOLE_HV;
    } else {
        WARNING("No console!");
    }

    if (!config->network.host_ip) {
        config->network.host_ip = DEFAULT_HOST_ADDR;
    }
    if (!config->network.guest_ip) {
        config->network.guest_ip = DEFAULT_GUEST_ADDR;
    }
    if (!config->network.host_mac) {
        config->network.host_mac = DEFAULT_HOST_MAC;
    }
    if (!config->network.guest_mac) {
        config->network.guest_mac = DEFAULT_GUEST_MAC;
    }
    if (!config->network.guest_name) {
        config->network.guest_name = DEFAULT_GUEST_NAME;
    }
    // init_list_init();
    return 0;
}

int kemu_run_work() {
    return 0;
}

void kemu_run_exit() {
    // init_list_exit();
}

int kemu_run(struct kvm_config *config) {
    int ret = -EFAULT;

    kemu_run_init(config);

    ret = kemu_run_work();
    kemu_run_exit();

    return ret;
}

int main(int argc, const char **argv) {
    memset(&kvm, 0, sizeof(struct kvm));
    argparse_option options[] = {
        // basic system boot options
        XBOX_ARG_STR(&kvm.cfg.system.name, NULL, "--name", "guest vm name", " <name>", "name"),
        XBOX_ARG_STR(&kvm.cfg.kernel.kernel_path, NULL, "--kernel", "kernel binary path", " <bzImage>", "kernel"),
        XBOX_ARG_STR(
            &kvm.cfg.kernel.kernel_cmdline, NULL, "--append", "kernel cmdline", " <cmdline>", "kernel-cmdline"),
        XBOX_ARG_STR(&kvm.cfg.drive.disk_path, NULL, "--disk", "disk path", " <disk>", "disk"),
        XBOX_ARG_STR(&kvm.cfg.memory.mem_size_str, "-m", NULL, "memory size", " <memory-size>", "memory"),
        XBOX_ARG_INT(&kvm.cfg.cpu.nrcpus, NULL, "--smp", "cpu number", " <cpus>", "cpu"),
        XBOX_ARG_STRS(&kvm.cfg.device.devices_str, NULL, "--device", "device" STORAGE_OPTION, " <device>", "device"),
        // storage options
        XBOX_ARG_STRS(&kvm.cfg.drive.drives_str, NULL, "--drive", "drive", " <driver>", "driver"),
        XBOX_ARG_STR(&kvm.cfg.drive.hda, NULL, "--hda", "harddisk", " <harddisk>", NULL),
        XBOX_ARG_STR(&kvm.cfg.drive.hdb, NULL, "--hdb", "harddisk", " <harddisk>", NULL),
        XBOX_ARG_STRS(&kvm.cfg.drive.cdrom, NULL, "--cdrom", "cdrom" NETWORK_OPTION, " <cdrom>", "cdrom"),
        // network options
        XBOX_ARG_STRS(&kvm.cfg.network.network, NULL, "--net", "network" DEBUG_OPTION, " <network>", "network"),
        // debug options
        XBOX_ARG_STR(&kvm.cfg.debug.log_file, "-D", NULL, "log file", " <log-file>", "log-file"),
        XBOX_ARG_STR(&kvm.cfg.debug.gdb_server, NULL, "--gdb", "gdb server", " tcp::<gdb-port>", "gdb-server"),
        XBOX_ARG_BOOLEAN(&kvm.cfg.debug.stop, "-S", NULL, "stop after boot", NULL, "stop"),
        XBOX_ARG_BOOLEAN(&kvm.cfg.debug.enable_gdb, "-s", NULL, "enable gdb server", NULL, "gdb"),
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
    kemu_validate_cfg(&kvm.cfg);
    kemu_run(&kvm.cfg);
    XBOX_free_argparse(&parser);
    return 0;
}