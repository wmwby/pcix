/* src/mem_read.c - 'mem read' command: parse args, validate BAR rules,
 * coordinate memmap + dump. */
#include "mem_read.h"
#include "memmap.h"
#include "dump.h"
#include "resource.h"
#include "bdf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint64_t offset;
    uint64_t size;
    int bar_num;
    dump_format_t format;
    int has_offset;
    int has_size;
    int has_all;
    int has_range;
    uint64_t range_start;
    uint64_t range_end;
    bdf_t bdf;
    int has_bdf;
} read_options_t;

/* Parse a hex range string "start-end" (e.g., "0x100-0x13f" or "100-13f").
 * Returns 1 on success, 0 on invalid format. */
static int parse_range(const char *str, uint64_t *start, uint64_t *end) {
    if (!str || !start || !end) return 0;

    char buf[128];
    if (strlen(str) >= sizeof(buf)) return 0;
    strcpy(buf, str);

    char *dash = strchr(buf, '-');
    if (!dash) return 0;

    *dash = '\0';
    char *endp1 = NULL, *endp2 = NULL;
    /* strtoull with base 16 accepts an optional 0x prefix */
    *start = strtoull(buf, &endp1, 16);
    *end = strtoull(dash + 1, &endp2, 16);

    if (endp1 == buf || *endp1 != '\0') return 0;
    if (endp2 == dash + 1 || *endp2 != '\0') return 0;

    return (*start <= *end) ? 1 : 0;
}

static void print_help(void) {
    printf("Usage: pcix mem read -s <bdf> --bar <n> [OPTIONS]\n\n");
    printf("Options:\n");
    printf("  -s <bdf>           PCI device BDF (required)\n");
    printf("  --bar <n>          BAR number to read, 0-5 (required)\n");
    printf("  --offset <addr>    Starting offset (hex)\n");
    printf("  --size <n>         Number of bytes to read (decimal)\n");
    printf("  --range <s-e>      Address range, e.g., 0x100-0x13f\n");
    printf("  --all              Read entire BAR\n");
    printf("  -b                 Display as bytes (default)\n");
    printf("  -w                 Display as 16-bit words\n");
    printf("  -d                 Display as 32-bit dwords\n");
    printf("  -h, --help         Show this help message\n");
}

int mem_cmd_read(int argc, char *argv[]) {
    read_options_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.format = FORMAT_BYTE;
    opts.bar_num = -1;

    /* Parse arguments. argv[0] is "read", so start at index 1. */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -s requires an argument\n");
                return 1;
            }
            if (!bdf_parse(argv[++i], &opts.bdf)) {
                fprintf(stderr, "Error: Invalid BDF format '%s'\n", argv[i]);
                return 1;
            }
            opts.has_bdf = 1;
        } else if (strcmp(argv[i], "--bar") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --bar requires an argument\n");
                return 1;
            }
            opts.bar_num = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--offset") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --offset requires an argument\n");
                return 1;
            }
            opts.offset = strtoull(argv[++i], NULL, 16);
            opts.has_offset = 1;
        } else if (strcmp(argv[i], "--size") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --size requires an argument\n");
                return 1;
            }
            opts.size = strtoull(argv[++i], NULL, 10);
            opts.has_size = 1;
        } else if (strcmp(argv[i], "--range") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --range requires an argument\n");
                return 1;
            }
            if (!parse_range(argv[++i], &opts.range_start, &opts.range_end)) {
                fprintf(stderr, "Error: Invalid range format. Expected start-end (e.g., 0x100-0x13f)\n");
                return 1;
            }
            opts.has_range = 1;
        } else if (strcmp(argv[i], "--all") == 0) {
            opts.has_all = 1;
        } else if (strcmp(argv[i], "-b") == 0) {
            opts.format = FORMAT_BYTE;
        } else if (strcmp(argv[i], "-w") == 0) {
            opts.format = FORMAT_WORD;
        } else if (strcmp(argv[i], "-d") == 0) {
            opts.format = FORMAT_DWORD;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        } else {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    /* Required arguments */
    if (!opts.has_bdf) {
        fprintf(stderr, "Error: -s <bdf> is required\n");
        return 1;
    }
    if (opts.bar_num < 0 || opts.bar_num > 5) {
        fprintf(stderr, "Error: --bar <n> is required (0-5)\n");
        return 1;
    }

    /* Device must exist */
    if (!bdf_device_exists(&opts.bdf)) {
        char bdf_str[32];
        snprintf(bdf_str, sizeof(bdf_str), "%04x:%02x:%02x.%d",
                 opts.bdf.domain, opts.bdf.bus, opts.bdf.device, opts.bdf.function);
        fprintf(stderr, "Error: PCI device %s not found\n", bdf_str);
        return 1;
    }

    /* 64-bit BAR rule: reject the high half of a 64-bit pair */
    if (resource_bar_is_64bit_high_half(&opts.bdf, opts.bar_num)) {
        fprintf(stderr, "Error: BAR%d is part of a 64-bit BAR pair with BAR%d. Use --bar %d instead.\n",
                opts.bar_num, opts.bar_num - 1, opts.bar_num - 1);
        return 1;
    }

    /* Resolve offset/size from --range or --all, else require offset+size */
    if (opts.has_range) {
        opts.offset = opts.range_start;
        opts.size = opts.range_end - opts.range_start + 1;
    } else if (opts.has_all) {
        resource_list_t resources;
        if (resource_read_all(&opts.bdf, &resources) != 0) {
            fprintf(stderr, "Error: Cannot read device resources\n");
            return 1;
        }
        int found = 0;
        for (int i = 0; i < resources.count; i++) {
            if (resources.entries[i].bar_num == opts.bar_num) {
                opts.offset = 0;
                opts.size = resources.entries[i].size;
                found = 1;
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "Error: BAR%d not found on this device\n", opts.bar_num);
            return 1;
        }
    } else if (!(opts.has_offset && opts.has_size)) {
        fprintf(stderr, "Error: Must specify --offset+--size, --range, or --all\n");
        return 1;
    }

    /* A size of 0 is invalid regardless of BAR. */
    if (opts.size == 0) {
        fprintf(stderr, "Error: Size must be greater than 0\n");
        return 1;
    }

    /* Bounds-check against the BAR size BEFORE opening /dev/mem, so an
     * out-of-range request is rejected without needing root privileges. */
    uint64_t bar_size = 0;
    {
        resource_list_t resources;
        if (resource_read_all(&opts.bdf, &resources) != 0) {
            fprintf(stderr, "Error: Cannot read device resources\n");
            return 1;
        }
        int found = 0;
        for (int i = 0; i < resources.count; i++) {
            if (resources.entries[i].bar_num == opts.bar_num) {
                bar_size = resources.entries[i].size;
                found = 1;
                break;
            }
        }
        if (!found || bar_size == 0) {
            fprintf(stderr, "Error: BAR%d is not enabled on this device\n", opts.bar_num);
            return 1;
        }
    }
    if (opts.size > bar_size || opts.offset > bar_size - opts.size) {
        char size_str[16];
        resource_format_size(bar_size, size_str, sizeof(size_str));
        fprintf(stderr, "Error: Read range exceeds BAR%d size (%s). Offset=0x%lx, Size=%lu\n",
                opts.bar_num, size_str,
                (unsigned long)opts.offset, (unsigned long)opts.size);
        return 1;
    }

    /* Open and mmap the BAR */
    memmap_region_t region;
    int rc = memmap_open_bar(&opts.bdf, opts.bar_num, &region);
    if (rc != 0) {
        switch (rc) {
            case -2:
                fprintf(stderr, "Error: BAR%d not found on this device\n", opts.bar_num);
                break;
            case -3:
                fprintf(stderr, "Error: BAR%d is not enabled on this device\n", opts.bar_num);
                break;
            case -4:
                fprintf(stderr, "Error: Permission denied opening /dev/mem. This command requires root privileges.\n");
                break;
            case -5:
                fprintf(stderr, "Error: mmap failed for BAR%d\n", opts.bar_num);
                break;
            default:
                fprintf(stderr, "Error: Cannot open BAR%d for reading\n", opts.bar_num);
        }
        return 1;
    }

    /* Allocate buffer and read */
    uint8_t *buf = malloc(opts.size);
    if (!buf) {
        fprintf(stderr, "Error: Cannot allocate %lu bytes\n", (unsigned long)opts.size);
        memmap_close(&region);
        return 1;
    }

    if (memmap_read(&region, opts.offset, buf, opts.size) != 0) {
        fprintf(stderr, "Error: Read failed at offset 0x%lx\n", (unsigned long)opts.offset);
        free(buf);
        memmap_close(&region);
        return 1;
    }

    dump_print(buf, opts.size, opts.offset, opts.format);

    free(buf);
    memmap_close(&region);
    return 0;
}
