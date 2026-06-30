/* src/mem.c */
#include "mem.h"
#include "resource.h"
#include <stdio.h>
#include <string.h>

int mem_cmd_show(const bdf_t *bdf) {
    resource_list_t list;
    int ret;

    /* Check if device exists */
    if (!bdf_device_exists(bdf)) {
        char bdf_str[32];
        snprintf(bdf_str, sizeof(bdf_str), "%04x:%02x:%02x.%d",
                bdf->domain, bdf->bus, bdf->device, bdf->function);
        fprintf(stderr, "Error: PCI device %s not found\n", bdf_str);
        return 1;
    }

    /* Read all resources */
    ret = resource_read_all(bdf, &list);
    if (ret != 0) {
        fprintf(stderr, "Error: Cannot read resource file (error %d)\n", ret);
        return 1;
    }

    /* Merge 64-bit BARs */
    resource_merge_64bit_bars(&list);

    /* Display results */
    int found = 0;
    for (int i = 0; i < list.count; i++) {
        resource_entry_t *entry = &list.entries[i];

        /* Skip disabled resources and merged entries */
        if (!entry->is_enabled || entry->bar_num == -2) {
            continue;
        }

        found++;

        char label[32];
        char size_str[32];
        char flags_str[128];

        /* Format label */
        if (entry->bar_num == -1) {
            snprintf(label, sizeof(label), "ROM");
        } else if (i < list.count - 1 && list.entries[i + 1].bar_num == -2) {
            /* 64-bit BAR - show as BAR0/1, BAR2/3, etc. */
            snprintf(label, sizeof(label), "BAR%d/%d", entry->bar_num, entry->bar_num + 1);
        } else {
            snprintf(label, sizeof(label), "BAR%d", entry->bar_num);
        }

        /* Format size and flags */
        resource_format_size(entry->size, size_str, sizeof(size_str));
        resource_format_flags(entry, flags_str, sizeof(flags_str));

        /* Print entry */
        printf("%-6s: 0x%016llx - 0x%016llx (%s, %s)\n",
               label,
               (unsigned long long)entry->start,
               (unsigned long long)entry->end,
               size_str,
               flags_str);
    }

    if (found == 0) {
        printf("No memory resources found for this device\n");
    }

    return 0;
}
