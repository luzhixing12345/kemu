#include <kvm/builtin-version.h>
#include <kvm/kvm-cmd.h>
#include <kvm/kvm.h>
#include <kvm/util.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

int kvm_cmd_version(int argc, const char **argv, const char *prefix) {
    printf("kvm tool %s\n", KVMTOOLS_VERSION);

    return 0;
}
