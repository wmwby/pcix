/* src/memmap.c */
#include "memmap.h"
#include "resource.h"
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define DEVMEM_PATH "/dev/mem"

/* Shared open body for read-only and read/write opens. */
static int open_bar_internal(const bdf_t *bdf, int bar_num, int writable,
                             memmap_region_t *region);

int memmap_open_bar(const bdf_t *bdf, int bar_num, memmap_region_t *region) {
    return open_bar_internal(bdf, bar_num, 0 /* read-only */, region);
}

int memmap_open_bar_rw(const bdf_t *bdf, int bar_num, memmap_region_t *region) {
    return open_bar_internal(bdf, bar_num, 1 /* read/write */, region);
}

static int open_bar_internal(const bdf_t *bdf, int bar_num, int writable,
                             memmap_region_t *region) {
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

    /* Open /dev/mem. O_SYNC makes MMIO writes synchronous (posted writes
     * are pushed to the device before the syscall returns). */
    region->fd = open(DEVMEM_PATH, writable ? (O_RDWR | O_SYNC) : (O_RDONLY | O_SYNC));
    if (region->fd < 0) {
        return -4;
    }

    /* mmap the region. PROT_WRITE only when opening for write. */
    int prot = PROT_READ | (writable ? PROT_WRITE : 0);
    region->addr = mmap(NULL, region->size, prot,
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

int memmap_write(const memmap_region_t *region, uint64_t offset,
                 const void *buf, size_t size) {
    /* Validate basic parameters */
    if (!region || !buf) {
        return -1;
    }

    /* Bounds check first (overflow-safe, same form as read):
     * size > region->size catches size overflow, then offset check covers
     * the end of the written range. */
    if (size > region->size || offset > region->size - size) {
        return -2;
    }

    /* Check if region is properly mapped */
    if (!region->addr) {
        return -1;
    }

    memcpy((uint8_t*)region->addr + offset, buf, size);

    /* Full memory barrier: flush posted writes so a subsequent read-back
     * from the same mapping observes the new values on x86/ARM-LE. */
    __sync_synchronize();
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
