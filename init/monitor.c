

#include "monitor.h"

#include <clib/clib.h>
#include <kvm/term.h>

#include "clib/keyboard.h"
#include "clib/shell.h"

static const char *monitor_prompt = "(kemu) ";
static char cmd_buf[1024];
static int cmd_buf_len = 0;

int term_printf(int term, const char *fmt, ...) {
    static char buf[1024];
    va_list ap;
    int len;

    va_start(ap, fmt);
    len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    return term_putc(buf, len, term);
}

int exec_cmd(int term) {
    cmd_buf[cmd_buf_len] = '\0';
    struct cmd_arg args;
    parseline(cmd_buf, &args);
    free_cmd_arg(&args);
    cmd_buf_len = 0;
    // parse and execute command
    return 0;
}

void monitor_add_cmdchar(char c, int term) {
    if (c == KEY_BACKSPACE) {
        if (cmd_buf_len > 0) {
            cmd_buf_len--;
        }
    } else if (c == KEY_ENTER || c == KEY_CR) {
        exec_cmd(term);
        term_printf(term, "\n%s", monitor_prompt);
    } else {
        term_printf(term, "%c", c);
        snprintf(cmd_buf + cmd_buf_len, sizeof(cmd_buf) - cmd_buf_len, "%c", c);
        cmd_buf_len++;
    }
}

void monitor_start(struct kvm *kvm, int term) {
    term_printf(term, "\nKEMU %s monitor - type 'help' for more information\n", CONFIG_VERSION);
    term_printf(term, "%s", monitor_prompt);
}

void monitor_end(struct kvm *kvm, int term) {
    term_printf(term, "\n");
}

int monitor_init(struct kvm *kvm) {
    return 0;
}
base_init(monitor_init);

int monitor_exit(struct kvm *kvm) {
    return 0;
}
base_exit(monitor_exit);