#include <sys/epoll.h>

#include "kvm/kvm.h"

typedef void (*epoll__event_handler_t)(struct kvm *kvm, struct epoll_event *ev);

struct kvm_epoll {
    int fd;
    int stop_fd;
    struct kvm *kvm;
    const char *name;
    pthread_t thread;
    epoll__event_handler_t handle_event;
};

int epoll_init(struct kvm *kvm, struct kvm_epoll *epoll, const char *name, epoll__event_handler_t handle_event);
int epoll__exit(struct kvm_epoll *epoll);
