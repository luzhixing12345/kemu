#ifndef __STRBUF_H__
#define __STRBUF_H__

#include <string.h>
#include <sys/types.h>

int prefixcmp(const char *str, const char *prefix);


/* some inline functions */

static inline const char *skip_prefix(const char *str, const char *prefix) {
    size_t len = strlen(prefix);
    return strncmp(str, prefix, len) ? NULL : str + len;
}

#endif
