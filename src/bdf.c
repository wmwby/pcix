/* src/bdf.c - BDF parsing and utilities */
#include "bdf.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define SYSFS_PCI_BASE "/sys/bus/pci/devices"

bool bdf_parse(const char *str, bdf_t *bdf) {
    if (!str || !bdf) {
        return false;
    }

    /* Initialize domain to 0 (default) */
    bdf->domain = 0;

    /* Try format with domain: DDDD:DD:DD.F (e.g., 0000:01:00.0) */
    int matched = sscanf(str, "%hx:%hhx:%hhx.%hhx",
                        &bdf->domain, &bdf->bus, &bdf->device, &bdf->function);

    if (matched == 4) {
        return true;
    }

    /* Try format without domain: DD:DD.F (e.g., 01:00.0) */
    bdf->domain = 0;
    matched = sscanf(str, "%hhx:%hhx.%hhx",
                    &bdf->bus, &bdf->device, &bdf->function);

    if (matched == 3) {
        return true;
    }

    return false;
}

bool bdf_to_path(const bdf_t *bdf, char *buf, size_t buflen) {
    if (!bdf || !buf || buflen < 64) {
        return false;
    }

    snprintf(buf, buflen, "%s/%04x:%02x:%02x.%d",
            SYSFS_PCI_BASE, bdf->domain, bdf->bus, bdf->device, bdf->function);

    return true;
}

bool bdf_device_exists(const bdf_t *bdf) {
    char path[128];

    if (!bdf_to_path(bdf, path, sizeof(path))) {
        return false;
    }

    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}
