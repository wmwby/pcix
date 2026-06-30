/* src/memmap.h */
#ifndef MEMMAP_H
#define MEMMAP_H

#include <stdint.h>
#include <stddef.h>
#include "bdf.h"

/* MAP_FAILED is from sys/mman.h but we need it for the interface */
#ifndef MAP_FAILED
#define MAP_FAILED ((void *) -1)
#endif

typedef struct {
    void *addr;           /* Mapped address */
    uint64_t phys_base;   /* Physical base address of BAR */
    uint64_t size;        /* Size of mapped region */
    int bar_num;          /* BAR number */
    int fd;               /* /dev/mem file descriptor */
} memmap_region_t;

/* Open and mmap a PCI device's BAR.
 * Returns 0 on success, negative on error.
 * Error codes:
 *   -1: Invalid parameters
 *   -2: Device not found
 *   -3: BAR not enabled
 *   -4: Cannot open /dev/mem (permission denied)
 *   -5: mmap failed
 */
int memmap_open_bar(const bdf_t *bdf, int bar_num, memmap_region_t *region);

/* Read from mapped region.
 * Bounds checking is performed on offset and size.
 * Returns 0 on success, negative on error.
 * Error codes:
 *   -1: Invalid parameters
 *   -2: Offset out of bounds
 */
int memmap_read(const memmap_region_t *region, uint64_t offset,
                void *buf, size_t size);

/* Unmap and close the region.
 * Safe to call with partially initialized region.
 */
void memmap_close(memmap_region_t *region);

#endif /* MEMMAP_H */
