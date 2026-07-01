/* src/mem_write.c - 'mem write' command: parse args, validate BAR rules,
 * coordinate memmap open (rw) + write + read-back. Mirrors mem_read.c. */
#include "mem_write.h"
#include "memmap.h"
#include "dump.h"
#include "resource.h"
#include "bdf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Which data source the user supplied. Exactly one is required. */
typedef enum {
    SRC_NONE,
    SRC_VALUE,
    SRC_FILE,
    SRC_HEX
} write_source_t;

typedef struct {
    uint64_t offset;
    int bar_num;
    dump_format_t format;       /* width selector for --value */
    bdf_t bdf;
    int has_bdf;
    int has_offset;
    int saw_width_flag;         /* true if -b/-w/-d was given */
    write_source_t source;
    uint64_t value;             /* SRC_VALUE */
    const char *file_path;      /* SRC_FILE */
    const char *hex_str;        /* SRC_HEX */
    int yes;                    /* --yes: skip confirmation */
    int no_verify;              /* --no-verify: skip read-back */
} write_options_t;

static void print_help(void) {
    printf("Usage: pcix mem write -s <bdf> --bar <n> --offset <addr> {--value <hex> | --file <path> | --hex <hexstr>} [OPTIONS]\n\n");
    printf("Write data to a PCI device BAR. Destructive -- requires root.\n\n");
    printf("Options:\n");
    printf("  -s <bdf>           PCI device BDF (required)\n");
    printf("  --bar <n>          BAR number to write, 0-5 (required)\n");
    printf("  --offset <addr>    Starting offset within the BAR, hex (required)\n");
    printf("  --value <hex>      Single register value to write (hex). Width set by -b/-w/-d\n");
    printf("  --file <path>      Write contents of this file\n");
    printf("  --hex <hexstr>     Write raw bytes from a hex string (e.g. deadbeef)\n");
    printf("  -b                 Value width = 8-bit (default for --value)\n");
    printf("  -w                 Value width = 16-bit\n");
    printf("  -d                 Value width = 32-bit\n");
    printf("  --yes, -y          Skip the confirmation prompt\n");
    printf("  --no-verify        Do not read back and print after writing\n");
    printf("  -h, --help         Show this help message\n");
}

/* Strictly parse a BAR number: a decimal integer in [0,5] with no trailing
 * junk. Returns 1 on success (sets *out), 0 on any rejection (empty,
 * non-numeric, "ROM", "3x", "-1", "6", etc.). Replaces atoi(), which silently
 * mapped "ROM" -> 0 and caused --bar ROM to write BAR 0. Mirrors mem_read.c. */
static int parse_bar_num(const char *str, int *out) {
    if (!str || !*str) return 0;
    char *endp = NULL;
    long v = strtol(str, &endp, 10);
    if (endp == str || *endp != '\0') return 0;  /* not a clean integer */
    if (v < 0 || v > 5) return 0;                 /* out of BAR range */
    *out = (int)v;
    return 1;
}

/* Decode a hex string ("deadbeef") into a freshly malloc'd byte buffer.
 * Returns 1 on success (sets *out + *out_len), 0 on invalid input
 * (odd length or non-hex char). Caller frees *out. */
static int decode_hex(const char *str, uint8_t **out, size_t *out_len) {
    if (!str || !out || !out_len) return 0;
    size_t slen = strlen(str);
    if (slen == 0 || (slen % 2) != 0) return 0;

    *out_len = slen / 2;
    *out = malloc(*out_len);
    if (!*out) return 0;

    for (size_t i = 0; i < *out_len; i++) {
        char hi = str[2 * i];
        char lo = str[2 * i + 1];
        int hv = -1, lv = -1;
        if (hi >= '0' && hi <= '9') hv = hi - '0';
        else if (hi >= 'a' && hi <= 'f') hv = hi - 'a' + 10;
        else if (hi >= 'A' && hi <= 'F') hv = hi - 'A' + 10;
        if (lo >= '0' && lo <= '9') lv = lo - '0';
        else if (lo >= 'a' && lo <= 'f') lv = lo - 'a' + 10;
        else if (lo >= 'A' && lo <= 'F') lv = lo - 'A' + 10;
        if (hv < 0 || lv < 0) {
            free(*out);
            *out = NULL;
            return 0;
        }
        (*out)[i] = (uint8_t)((hv << 4) | lv);
    }
    return 1;
}

/* Read an entire file into a freshly malloc'd buffer.
 * Returns 1 on success (sets *out + *out_len), 0 on failure. */
static int read_file_all(const char *path, uint8_t **out, size_t *out_len) {
    if (!path || !out || !out_len) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return 0; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return 0; }

    *out_len = (size_t)sz;
    *out = malloc(*out_len ? *out_len : 1);
    if (!*out) { fclose(f); return 0; }

    if (*out_len > 0 && fread(*out, 1, *out_len, f) != *out_len) {
        free(*out);
        *out = NULL;
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

/* Return the byte width implied by the format flag. */
static int format_width(dump_format_t f) {
    switch (f) {
    case FORMAT_DWORD: return 4;
    case FORMAT_WORD:  return 2;
    case FORMAT_BYTE:
    default:           return 1;
    }
}

/* Print a write summary and prompt stdin for confirmation.
 * Returns 1 if the user confirmed, 0 otherwise (including EOF/non-tty). */
static int confirm_write(const write_options_t *opts, size_t data_size,
                         const uint8_t *data) {
    char bdf_str[32];
    snprintf(bdf_str, sizeof(bdf_str), "%04x:%02x:%02x.%d",
             opts->bdf.domain, opts->bdf.bus, opts->bdf.device, opts->bdf.function);

    fprintf(stderr, "About to write %zu byte(s) to BAR%d of %s at offset 0x%lx:\n",
            data_size, opts->bar_num, bdf_str, (unsigned long)opts->offset);
    /* Show up to 16 bytes of what will be written. */
    size_t show = data_size < 16 ? data_size : 16;
    fprintf(stderr, "  data:");
    for (size_t i = 0; i < show; i++) fprintf(stderr, " %02x", data[i]);
    if (data_size > 16) fprintf(stderr, " ...");
    fprintf(stderr, "\n  Continue? [y/N] ");
    fflush(stderr);

    char line[16];
    if (!fgets(line, sizeof(line), stdin)) {
        fprintf(stderr, "Aborted (no input).\n");
        return 0;
    }
    /* Trim leading whitespace. */
    char *p = line;
    while (*p && isspace((unsigned char)*p)) p++;
    char c = (char)tolower((unsigned char)*p);
    if (c == 'y') {
        /* Accept "y" or "yes". */
        return 1;
    }
    fprintf(stderr, "Aborted.\n");
    return 0;
}

int mem_cmd_write(int argc, char *argv[]) {
    write_options_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.format = FORMAT_BYTE;
    opts.bar_num = -1;

    /* Parse arguments. argv[0] is "write", so start at index 1. */
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
            const char *bararg = argv[++i];
            if (!parse_bar_num(bararg, &opts.bar_num)) {
                fprintf(stderr, "Error: Invalid --bar value '%s' (expected 0-5)\n", bararg);
                return 1;
            }
        } else if (strcmp(argv[i], "--offset") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --offset requires an argument\n");
                return 1;
            }
            opts.offset = strtoull(argv[++i], NULL, 16);
            opts.has_offset = 1;
        } else if (strcmp(argv[i], "--value") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --value requires an argument\n");
                return 1;
            }
            opts.value = strtoull(argv[++i], NULL, 16);
            opts.source = SRC_VALUE;
        } else if (strcmp(argv[i], "--file") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --file requires an argument\n");
                return 1;
            }
            opts.file_path = argv[++i];
            opts.source = SRC_FILE;
        } else if (strcmp(argv[i], "--hex") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --hex requires an argument\n");
                return 1;
            }
            opts.hex_str = argv[++i];
            opts.source = SRC_HEX;
        } else if (strcmp(argv[i], "-b") == 0) {
            opts.format = FORMAT_BYTE;
            opts.saw_width_flag = 1;
        } else if (strcmp(argv[i], "-w") == 0) {
            opts.format = FORMAT_WORD;
            opts.saw_width_flag = 1;
        } else if (strcmp(argv[i], "-d") == 0) {
            opts.format = FORMAT_DWORD;
            opts.saw_width_flag = 1;
        } else if (strcmp(argv[i], "--yes") == 0 || strcmp(argv[i], "-y") == 0) {
            opts.yes = 1;
        } else if (strcmp(argv[i], "--no-verify") == 0) {
            opts.no_verify = 1;
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
    if (!opts.has_offset) {
        fprintf(stderr, "Error: --offset <addr> is required\n");
        return 1;
    }
    if (opts.source == SRC_NONE) {
        fprintf(stderr, "Error: Must specify one of --value, --file, or --hex\n");
        return 1;
    }

    /* -b/-w/-d only apply to --value. */
    if (opts.saw_width_flag && opts.source != SRC_VALUE) {
        fprintf(stderr, "Warning: -b/-w/-d only apply to --value; ignoring for this data source.\n");
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

    /* Resolve the data to write into a buffer + size. */
    uint8_t *data = NULL;
    size_t data_size = 0;

    if (opts.source == SRC_VALUE) {
        int width = format_width(opts.format);
        uint64_t max_val = (width == 8) ? 0xffffffffffffffffULL
                          : ((uint64_t)1 << (8 * width)) - 1;
        if (opts.value > max_val) {
            fprintf(stderr, "Error: Value 0x%lx exceeds %d-bit width (max 0x%llx). Use a larger -w/-d width.\n",
                    (unsigned long)opts.value, 8 * width,
                    (unsigned long long)max_val);
            return 1;
        }
        data_size = (size_t)width;
        data = malloc(data_size);
        if (!data) {
            fprintf(stderr, "Error: Cannot allocate %zu bytes\n", data_size);
            return 1;
        }
        /* Little-endian encoding: low byte first. Matches the LE register
         * convention used by dump_print, so a read-back with the same flag
         * shows the value the user wrote. */
        for (int k = 0; k < width; k++) {
            data[k] = (uint8_t)((opts.value >> (8 * k)) & 0xff);
        }
    } else if (opts.source == SRC_HEX) {
        if (!decode_hex(opts.hex_str, &data, &data_size)) {
            fprintf(stderr, "Error: Invalid --hex string (must be even-length hex, e.g. deadbeef)\n");
            return 1;
        }
    } else { /* SRC_FILE */
        if (!read_file_all(opts.file_path, &data, &data_size)) {
            fprintf(stderr, "Error: Cannot read file '%s'\n", opts.file_path);
            return 1;
        }
        if (data_size == 0) {
            fprintf(stderr, "Error: File '%s' is empty\n", opts.file_path);
            free(data);
            return 1;
        }
    }

    /* Bounds-check against the BAR size BEFORE opening /dev/mem, so an
     * out-of-range request is rejected without needing root privileges. */
    uint64_t bar_size = 0;
    {
        resource_list_t resources;
        if (resource_read_all(&opts.bdf, &resources) != 0) {
            fprintf(stderr, "Error: Cannot read device resources\n");
            free(data);
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
            free(data);
            return 1;
        }
    }
    if (data_size > bar_size || opts.offset > bar_size - data_size) {
        char size_str[16];
        resource_format_size(bar_size, size_str, sizeof(size_str));
        fprintf(stderr, "Error: Write range exceeds BAR%d size (%s). Offset=0x%lx, Size=%zu\n",
                opts.bar_num, size_str,
                (unsigned long)opts.offset, data_size);
        free(data);
        return 1;
    }

    /* Confirmation prompt unless --yes. */
    if (!opts.yes) {
        if (!confirm_write(&opts, data_size, data)) {
            free(data);
            return 1;
        }
    }

    /* Open and mmap the BAR for write. */
    memmap_region_t region;
    int rc = memmap_open_bar_rw(&opts.bdf, opts.bar_num, &region);
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
            fprintf(stderr, "Error: Cannot open BAR%d for writing\n", opts.bar_num);
        }
        free(data);
        return 1;
    }

    /* Perform the write. */
    if (memmap_write(&region, opts.offset, data, data_size) != 0) {
        fprintf(stderr, "Error: Write failed at offset 0x%lx\n", (unsigned long)opts.offset);
        free(data);
        memmap_close(&region);
        return 1;
    }

    fprintf(stderr, "Wrote %zu byte(s) to BAR%d at offset 0x%lx.\n",
            data_size, opts.bar_num, (unsigned long)opts.offset);

    /* Read back and print, unless --no-verify. */
    int ret = 0;
    if (!opts.no_verify) {
        uint8_t *rbuf = malloc(data_size);
        if (!rbuf) {
            fprintf(stderr, "Error: Cannot allocate %zu bytes for read-back\n", data_size);
            /* Write succeeded; treat read-back alloc failure as non-fatal. */
        } else if (memmap_read(&region, opts.offset, rbuf, data_size) != 0) {
            fprintf(stderr, "Error: Read-back failed at offset 0x%lx\n", (unsigned long)opts.offset);
            free(rbuf);
        } else {
            dump_print(rbuf, data_size, opts.offset, opts.format);
            free(rbuf);
        }
    }

    free(data);
    memmap_close(&region);
    return ret;
}
