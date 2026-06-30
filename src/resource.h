/* src/resource.h */
#ifndef RESOURCE_H
#define RESOURCE_H

#include <stdint.h>
#include <stdbool.h>
#include "bdf.h"

#define MAX_RESOURCES 7

/* Resource flags from resource file */
#define RESOURCE_FLAG_IO        (1 << 0)
#define RESOURCE_FLAG_MEM       (1 << 1)
#define RESOURCE_FLAG_PREFETCH  (1 << 2)
#define RESOURCE_FLAG_64BIT     (1 << 3)

typedef struct {
    int bar_num;                /* BAR number (0-5), or -1 for ROM */
    uint64_t start;             /* Start address */
    uint64_t end;               /* End address */
    uint64_t size;              /* Size in bytes */
    int flags;                  /* Resource flags */
    bool is_64bit;              /* True if 64-bit BAR */
    bool is_prefetchable;       /* True if prefetchable */
    bool is_enabled;            /* True if resource is enabled (size > 0) */
} resource_entry_t;

typedef struct {
    resource_entry_t entries[MAX_RESOURCES];
    int count;
} resource_list_t;

/* Read all resources for a PCI device from sysfs.
 * Returns 0 on success, negative on error.
 */
int resource_read_all(const bdf_t *bdf, resource_list_t *list);

/* Format size for human reading (e.g., "1M", "512K").
 * Caller must provide buffer of at least 16 bytes.
 */
void resource_format_size(uint64_t size, char *buf, size_t buflen);

/* Format flags for human reading.
 * Caller must provide buffer of at least 64 bytes.
 */
void resource_format_flags(const resource_entry_t *entry, char *buf, size_t buflen);

/* Detect and merge 64-bit BAR pairs.
 * If BAR N is 64-bit and BAR N+1 exists, merge them.
 */
void resource_merge_64bit_bars(resource_list_t *list);

#endif /* RESOURCE_H */
