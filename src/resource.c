/* src/resource.c */
#include "resource.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static uint64_t parse_hex_or_zero(const char *str) {
    if (!str || !*str) return 0;
    return strtoull(str, NULL, 16);
}

/* Read a single line from a file */
static char *read_file_line(const char *path, char *buf, size_t buflen) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    char *result = fgets(buf, buflen, f);
    fclose(f);

    if (result) {
        /* Remove trailing newline */
        size_t len = strlen(result);
        if (len > 0 && result[len-1] == '\n') {
            result[len-1] = '\0';
        }
    }

    return result;
}

int resource_read_all(const bdf_t *bdf, resource_list_t *list) {
    char path[256];
    char line[512];

    if (!bdf || !list) {
        return -1;
    }

    memset(list, 0, sizeof(*list));

    /* Build path to resource file */
    snprintf(path, sizeof(path),
             "/sys/bus/pci/devices/%04x:%02x:%02x.%d/resource",
             bdf->domain, bdf->bus, bdf->device, bdf->function);

    FILE *f = fopen(path, "r");
    if (!f) {
        return -2;
    }

    int idx = 0;
    while (fgets(line, sizeof(line), f) && idx < MAX_RESOURCES) {
        uint64_t start, end, flags;

        /* Parse line: "start end flags" */
        int parsed = sscanf(line, "0x%llx 0x%llx 0x%llx",
                           (unsigned long long*)&start,
                           (unsigned long long*)&end,
                           (unsigned long long*)&flags);

        if (parsed == 3) {
            list->entries[idx].bar_num = (idx < 6) ? idx : -1;
            list->entries[idx].start = start;
            list->entries[idx].end = end;
            list->entries[idx].size = (end > start) ? (end - start + 1) : 0;
            list->entries[idx].flags = (int)flags;
            list->entries[idx].is_64bit = (flags & RESOURCE_FLAG_64BIT) != 0;
            list->entries[idx].is_prefetchable = (flags & RESOURCE_FLAG_PREFETCH) != 0;
            list->entries[idx].is_enabled = (list->entries[idx].size > 0);
            idx++;
        }
    }

    fclose(f);
    list->count = idx;
    return 0;
}

void resource_format_size(uint64_t size, char *buf, size_t buflen) {
    const char *units[] = {"B", "K", "M", "G", "T"};
    int unit_idx = 0;
    double value = (double)size;

    while (value >= 1024 && unit_idx < 4) {
        value /= 1024;
        unit_idx++;
    }

    if (value == (int)value) {
        snprintf(buf, buflen, "%.0f%s", value, units[unit_idx]);
    } else {
        snprintf(buf, buflen, "%.1f%s", value, units[unit_idx]);
    }
}

void resource_format_flags(const resource_entry_t *entry, char *buf, size_t buflen) {
    if (entry->bar_num == -1) {
        /* ROM */
        snprintf(buf, buflen, "read-only");
        return;
    }

    int flags = entry->flags;
    int pos = 0;

    if (flags & RESOURCE_FLAG_IO) {
        pos += snprintf(buf + pos, buflen - pos, "io-port");
    } else if (flags & RESOURCE_FLAG_MEM) {
        if (entry->is_prefetchable) {
            pos += snprintf(buf + pos, buflen - pos, "prefetchable");
        } else {
            pos += snprintf(buf + pos, buflen - pos, "non-prefetchable");
        }

        if (entry->is_64bit) {
            pos += snprintf(buf + pos, buflen - pos, ", 64-bit");
        } else {
            pos += snprintf(buf + pos, buflen - pos, ", 32-bit");
        }
    } else {
        snprintf(buf, buflen, "unknown");
    }
}

void resource_merge_64bit_bars(resource_list_t *list) {
    if (!list || list->count < 2) return;

    for (int i = 0; i < list->count - 1; i++) {
        resource_entry_t *curr = &list->entries[i];
        resource_entry_t *next = &list->entries[i + 1];

        /* Check if current BAR is 64-bit and next BAR looks like its high part */
        if (curr->is_64bit && curr->is_enabled) {
            /* Check if next BAR is effectively part of the 64-bit address */
            if (next->start == 0 && next->end == 0 && next->size == 0) {
                /* Mark next as merged - it will be skipped in display */
                curr->bar_num = i; /* Keep original number */
                next->bar_num = -2; /* Mark as merged */
            }
        }
    }
}
