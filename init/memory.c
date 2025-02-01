
#include "memory.h"

#include <ctype.h>
#include <linux/sizes.h>
#include <stdio.h>
#include <unistd.h>

#include "clib/clib.h"

#define RAM_SIZE_RATIO 0.8
#define MIN_RAM_SIZE   SZ_64M

static long host_ram_nrpages(void) {
    long nr_pages = sysconf(_SC_PHYS_PAGES);

    if (nr_pages < 0) {
        WARNING("sysconf(_SC_PHYS_PAGES) failed");
        return 0;
    }

    return nr_pages;
}

static long host_page_size(void) {
    long page_size = sysconf(_SC_PAGE_SIZE);

    if (page_size < 0) {
        WARNING("sysconf(_SC_PAGE_SIZE) failed");
        return 0;
    }

    return page_size;
}

u64 get_ram_size(int nr_cpus) {
    long nr_pages_available = host_ram_nrpages() * RAM_SIZE_RATIO;
    u64 ram_size = (u64)SZ_64M * (nr_cpus + 3);
    u64 available = MIN_RAM_SIZE;

    if (nr_pages_available)
        available = nr_pages_available * host_page_size();

    if (ram_size > available)
        ram_size = available;

    return ram_size;
}

u64 parse_ram_size(char *ram_size_str) {
    u64 ram_size = 0;
    char unit = '\0';
    int num_converted = 0;

    // Check if the input string is empty or null
    if (ram_size_str == NULL || ram_size_str[0] == '\0') {
        return 0;
    }

    // Scan the number and unit from the string
    num_converted = sscanf(ram_size_str, "%llu%c", &ram_size, &unit);

    // If sscanf did not extract a valid number, return 0
    if (num_converted != 2 && num_converted != 1) {
        WARNING("Mem size format should be [0-9]+[KMG]?, like 4K or 1G");
        return 0;
    }

    // Determine the unit and calculate the size
    switch (toupper(unit)) {
        case 'K':
            ram_size *= KB;  // K = Kilobytes
            break;
        case 'M':
            ram_size *= MB;  // M = Megabytes
            break;
        case 'G':
            ram_size *= GB;  // G = Gigabytes
            break;
        case '\0':
            // No unit provided, assume the value is in bytes
            break;
        default:
            // Invalid unit, return 0
            return 0;
    }
    return ram_size;
}