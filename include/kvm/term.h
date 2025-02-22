#ifndef KVM__TERM_H
#define KVM__TERM_H

#include <stdbool.h>
#include <sys/uio.h>

#include "kvm/kvm.h"

#define CONSOLE_8250   1
#define CONSOLE_VIRTIO 2
#define CONSOLE_HV     3

#define TERM_MAX_DEVS  4

int term_putc_iov(struct iovec *iov, int iovcnt, int term);
int term_getc_iov(struct kvm *kvm, struct iovec *iov, int iovcnt, int term);
int term_putc(char *addr, int cnt, int term);
int term_getc(struct kvm *kvm, int term);

bool term_readable(int term);
int tty_parser(const struct option *opt, const char *arg, int unset);

#endif /* KVM__TERM_H */
