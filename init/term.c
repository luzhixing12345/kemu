#include "kvm/term.h"

#include <poll.h>
#include <pty.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/uio.h>
#include <termios.h>
#include <unistd.h>
#include <utmp.h>

#include "kvm/kvm-cpu.h"
#include "kvm/kvm.h"
#include "kvm/read-write.h"
#include "kvm/util.h"
#include "monitor.h"

#define TERM_FD_IN  0
#define TERM_FD_OUT 1

static struct termios g_orig_term;

static int term_fds[TERM_MAX_DEVS][2];

static pthread_t term_poll_thread;

/* ctrl-a is used for escape */
#define term_escape_char 0x01

static bool term_in_escape = false;
static bool term_in_monitor = false;

// return 0 if continue, -1 if not char
// c may be changed
static int term_monitor_check(struct kvm *kvm, int term, unsigned char *c) {
    if (term_in_escape) {
        term_in_escape = false;
        // ctrl-a + x: exit vm
        if (*c == 'x') {
            kvm_vm_exit(kvm);
        } else if (*c == 'c') {
            if (!term_in_monitor) {
                term_in_monitor = true;
                monitor_start(kvm, term);
                return -1;
            } else {
                // end monitor
                term_in_monitor = false;
                monitor_end(kvm, term);
                *c = KEY_ENTER;
                return 0;
            }
        }
        if (*c == term_escape_char)
            return 0;
    }

    if (*c == term_escape_char) {
        term_in_escape = true;
        return -1;
    }

    if (term_in_monitor) {
        monitor_add_cmdchar(*c, term);
        return -1;
    }

    return 0;
}

int term_getc(struct kvm *kvm, int term) {
    unsigned char c;

    if (read_in_full(term_fds[term][TERM_FD_IN], &c, 1) < 0)
        return -1;

    if (term_monitor_check(kvm, term, &c) == -1)
        return -1;
    return c;
}

int term_putc(char *addr, int cnt, int term) {
    int ret;
    int num_remaining = cnt;

    while (num_remaining) {
        ret = write(term_fds[term][TERM_FD_OUT], addr, num_remaining);
        if (ret < 0)
            return cnt - num_remaining;
        num_remaining -= ret;
        addr += ret;
    }

    return cnt;
}

int term_getc_iov(struct kvm *kvm, struct iovec *iov, int iovcnt, int term) {
    int c;

    c = term_getc(kvm, term);

    if (c < 0)
        return 0;

    *((char *)iov[TERM_FD_IN].iov_base) = (char)c;

    return sizeof(char);
}

int term_putc_iov(struct iovec *iov, int iovcnt, int term) {
    return writev(term_fds[term][TERM_FD_OUT], iov, iovcnt);
}

bool term_readable(int term) {
    struct pollfd pollfd = (struct pollfd){
        .fd = term_fds[term][TERM_FD_IN],
        .events = POLLIN,
        .revents = 0,
    };
    int err;

    err = poll(&pollfd, 1, 0);
    return (err > 0 && (pollfd.revents & POLLIN));
}

static void *term_poll_thread_loop(void *param) {
    struct pollfd fds[TERM_MAX_DEVS];
    struct kvm *kvm = (struct kvm *)param;
    int i;

    kvm_set_thread_name("term-poll");

    for (i = 0; i < TERM_MAX_DEVS; i++) {
        fds[i].fd = term_fds[i][TERM_FD_IN];
        fds[i].events = POLLIN;
        fds[i].revents = 0;
    }

    while (1) {
        /* Poll with infinite timeout */
        if (poll(fds, TERM_MAX_DEVS, -1) < 1)
            break;
        kvm_arch_read_term(kvm);
    }

    die("term_poll_thread_loop: error polling device fds %d\n", errno);
    return NULL;
}

static void term_cleanup(void) {
    int i;

    for (i = 0; i < TERM_MAX_DEVS; i++) tcsetattr(term_fds[i][TERM_FD_IN], TCSANOW, &g_orig_term);
}

static void term_sig_cleanup(int sig) {
    term_cleanup();
    signal(sig, SIG_DFL);
    raise(sig);
}

static void term_set_tty(int term) {
    struct termios orig_term;
    int master, slave;
    char new_pty[PATH_MAX];

    if (tcgetattr(STDIN_FILENO, &orig_term) < 0)
        die("unable to save initial standard input settings");

    orig_term.c_lflag &= ~(ICANON | ECHO | ISIG);

    if (openpty(&master, &slave, new_pty, &orig_term, NULL) < 0)
        return;

    close(slave);

    DEBUG("Assigned terminal %d to pty %s\n", term, new_pty);

    term_fds[term][TERM_FD_IN] = term_fds[term][TERM_FD_OUT] = master;
}

int tty_parser(const struct option *opt, const char *arg, int unset) {
    int tty = atoi(arg);

    term_set_tty(tty);

    return 0;
}

static int term_init(struct kvm *kvm) {
    struct termios term;
    int i, r;

    for (i = 0; i < TERM_MAX_DEVS; i++)
        if (term_fds[i][TERM_FD_IN] == 0) {
            term_fds[i][TERM_FD_IN] = STDIN_FILENO;
            term_fds[i][TERM_FD_OUT] = STDOUT_FILENO;
        }

    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))
        return 0;

    r = tcgetattr(STDIN_FILENO, &g_orig_term);
    if (r < 0) {
        pr_warning("unable to save initial standard input settings");
        return r;
    }

    term = g_orig_term;
    term.c_iflag &= ~(ICRNL);
    term.c_lflag &= ~(ICANON | ECHO | ISIG);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);

    /* Use our own blocking thread to read stdin, don't require a tick */
    if (pthread_create(&term_poll_thread, NULL, term_poll_thread_loop, kvm))
        die("Unable to create console input poll thread\n");

    signal(SIGTERM, term_sig_cleanup);
    atexit(term_cleanup);

    return 0;
}
dev_init(term_init);

static int term_exit(struct kvm *kvm) {
    return 0;
}
dev_exit(term_exit);
