
#pragma once

#include <linux/types.h>

#define KB (1024)
#define MB (1024 * 1024)
#define GB (1024 * 1024 * 1024)

u64 get_ram_size(int nr_cpus);
u64 parse_ram_size(char *ram_size_str);