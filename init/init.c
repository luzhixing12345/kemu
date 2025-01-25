

#include <clib/clib.h>
#include <kvm/init.h>
#include <linux/list.h>
#include <vm/vm.h>

static struct hlist_head init_lists[MODULE_MAX];
static struct hlist_head exit_lists[MODULE_MAX];

int init_list_init(struct vm *vm) {
    unsigned int i;
    int r = 0;
    struct init_item *t;

    for (i = 0; i < ARRAY_SIZE(init_lists); i++) hlist_for_each_entry(t, &init_lists[i], n) {
            // DEBUG("call init: %s\n", t->fn_name);
            r = t->init(vm);
            if (r < 0) {
                WARNING("Failed init: %s\n", t->fn_name);
                goto fail;
            }
        }

fail:
    return r;
}

int init_list_exit(struct vm *vm) {
    int i;
    int r = 0;
    struct init_item *t;

    for (i = ARRAY_SIZE(exit_lists) - 1; i >= 0; i--) hlist_for_each_entry(t, &exit_lists[i], n) {
            // DEBUG("call exit: %s\n", t->fn_name);
            r = t->init(vm);
            if (r < 0) {
                WARNING("%s failed.\n", t->fn_name);
                goto fail;
            }
        }
fail:
    return r;
}

int exit_list_add(struct init_item *t, int (*init)(), init_type type, const char *name) {
    t->init = init;
    t->fn_name = name;
    hlist_add_head(&t->n, &exit_lists[type]);
    // DEBUG("register exit fn: %s\n", name);
    return 0;
}

int init_list_add(struct init_item *t, int (*init)(struct vm *), init_type type, const char *name) {
    t->init = init;
    t->fn_name = name;
    hlist_add_head(&t->n, &init_lists[type]);
    // DEBUG("register init[%d] fn: %s\n", type, name);
    return 0;
}
