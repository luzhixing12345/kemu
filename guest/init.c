/*
 * This is a simple init for shared rootfs guests. This part should be limited
 * to doing mounts and running stage 2 of the init process.
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static int run_process(char *filename) {
    char *new_argv[] = {filename, NULL};
    char *new_env[] = {"TERM=linux", "DISPLAY=192.168.33.1:0", "HOME=/virt/home", NULL};

    return execve(filename, new_argv, new_env);
}

static int run_process_sandbox(char *filename) {
    char *new_argv[] = {filename, "/virt/sandbox.sh", NULL};
    char *new_env[] = {"TERM=linux", "HOME=/virt/home", NULL};

    return execve(filename, new_argv, new_env);
}

static void do_mounts(void) {
#ifndef CONFIG_GUEST_PRE_INIT
    mount("hostfs", "/host", "9p", MS_RDONLY, "trans=virtio,version=9p2000.L");
#endif
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("proc", "/proc", "proc", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    mkdir("/dev/pts", 0755);
    mount("devpts", "/dev/pts", "devpts", 0, NULL);
}

int main(int argc, char *argv[]) {
    pid_t child;
    int status;

    puts("Mounting...");

    do_mounts();

    /* get session leader */
    setsid();

    /* set controlling terminal */
    ioctl(0, TIOCSCTTY, 1);

    child = fork();
    if (child < 0) {
        printf("Fatal: fork() failed with %d\n", child);
        return 0;
    } else if (child == 0) {
        if (access("/virt/sandbox.sh", R_OK) == 0)
            run_process_sandbox("/bin/sh");
        else
            run_process("/bin/sh");
    } else {
        pid_t corpse;

        do {
            corpse = waitpid(-1, &status, 0);
        } while (corpse != child);
    }

    sync();
    reboot(RB_AUTOBOOT);

    printf("Init failed: %s\n", strerror(errno));

    return 0;
}
