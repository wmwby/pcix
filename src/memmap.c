/* src/memmap.c */
#include "memmap.h"
#include "resource.h"
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define DEVMEM_PATH "/dev/mem"

int memmap_open_bar(const bdf_t *bdf, int bar_num, memmap_region_t *region) {
    /* Validate parameters */
    if (!bdf || !region || bar_num < 0 || bar_num > 5) {
        return -1;
    }

    /* Initialize region to zero */
    memset(region, 0, sizeof(*region));
    region->bar_num = bar_num;
    region->fd = -1;

    /* Check if device exists */
    if (!bdf_device_exists(bdf)) {
        return -2;
    }

    /* Get BAR information from sysfs */
    resource_list_t resources;
    if (resource_read_all(bdf, &resources) != 0) {
        return -2;
    }

    /* Find the requested BAR */
    resource_entry_t *bar_entry = NULL;
    for (int i = 0; i < resources.count; i++) {
        if (resources.entries[i].bar_num == bar_num) {
            bar_entry = &resources.entries[i];
            break;
        }
    }

    if (!bar_entry) {
        return -2;
    }

    /* Check if BAR is enabled */
    if (!bar_entry->is_enabled) {
        return -3;
    }

    region->phys_base = bar_entry->start;
    region->size = bar_entry->size;

    /* Open /dev/mem */
    region->fd = open(DEVMEM_PATH, O_RDONLY | O_SYNC);
    if (region->fd < 0) {
        return -4;
    }

    /* mmap the region */
    region->addr = mmap(NULL, region->size, PROT_READ,
                        MAP_SHARED, region->fd, region->phys_base);

    if (region->addr == MAP_FAILED) {
        close(region->fd);
        region->fd = -1;
        region->addr = NULL;
        return -5;
    }

    return 0;
}

int memmap_read(const memmap_region_t *region, uint64_t offset,
                void *buf, size_t size) {
    /* Validate basic parameters */
    if (!region || !buf) {
        return -1;
    }

    /* Check bounds first (semantic validation)
     * Use overflow-safe comparison: size > region->size catches size overflow,
     * then offset > region->size - size catches offset overflow and bounds. */
    if (size > region->size || offset > region->size - size) {
        return -2;
    }

    /* Check if region is properly mapped */
    if (!region->addr) {
        return -1;
    }

    memcpy(buf, (uint8_t*)region->addr + offset, size);
    return 0;
}

void memmap_close(memmap_region_t *region) {
    if (!region) return;

    if (region->addr != NULL && region->addr != MAP_FAILED) {
        munmap(region->addr, region->size);
        region->addr = NULL;
    }

    if (region->fd >= 0) {
        close(region->fd);
        region->fd = -1;
    }
}
