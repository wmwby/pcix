/* src/dump.c */
#include "dump.h"
#include <stdio.h>
#include <ctype.h>

/* Check if character is printable */
static int is_printable_ascii(int c) {
    return (c >= 32 && c <= 126);
}

/* Print ASCII representation */
static void print_ascii(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (is_printable_ascii(data[i])) {
            putchar(data[i]);
        } else {
            putchar('.');
        }
    }
}

void dump_print(const uint8_t *data, size_t size,
                uint64_t base_offset, dump_format_t format) {
    size_t units_per_line;
    size_t unit_size;
    const char *fmt_str;

    switch (format) {
    case FORMAT_WORD:
        units_per_line = 8;
        unit_size = 2;
        fmt_str = "%04x";
        break;
    case FORMAT_DWORD:
        units_per_line = 4;
        unit_size = 4;
        fmt_str = "%08x";
        break;
    case FORMAT_BYTE:
    default:
        units_per_line = 16;
        unit_size = 1;
        fmt_str = "%02x";
        break;
    }

    size_t bytes_per_line = units_per_line * unit_size;

    /* Announce the interpretation so the user knows how to read -w/-d.
     * Emitted to stderr so stdout stays a pure hexdump for piping.
     * -b is raw bytes in memory order; -w/-d group bytes into 16/32-bit
     * CPU register values in little-endian (low address = LSB), matching
     * a volatile read on x86/ARM-LE hosts. */
    switch (format) {
    case FORMAT_WORD:
        fprintf(stderr, "# word (16-bit), little-endian (CPU register value)\n");
        break;
    case FORMAT_DWORD:
        fprintf(stderr, "# dword (32-bit), little-endian (CPU register value)\n");
        break;
    case FORMAT_BYTE:
    default:
        fprintf(stderr, "# byte, memory order (raw bytes)\n");
        break;
    }

    for (size_t i = 0; i < size; i += bytes_per_line) {
        /* Print offset */
        printf("%08lx  ", (unsigned long)(base_offset + i));

        /* Print hex values */
        size_t remaining = size - i;
        size_t line_bytes = (remaining < bytes_per_line) ? remaining : bytes_per_line;

        for (size_t j = 0; j < line_bytes; j += unit_size) {
            if (j + unit_size <= line_bytes) {
                /* Print full unit. Little-endian: low address is the least
                 * significant byte, matching a volatile read on x86/ARM-LE. */
                uint32_t val = 0;
                for (size_t k = 0; k < unit_size; k++) {
                    val |= (uint32_t)data[i + j + k] << (8 * k);
                }
                printf(fmt_str, val);
                if (j + unit_size == bytes_per_line / 2) {
                    putchar(' ');
                } else {
                    putchar(' ');
                }
            } else {
                /* Padding for incomplete line */
                for (size_t k = 0; k < unit_size * 2; k++) {
                    putchar(' ');
                }
                putchar(' ');
            }
        }

        /* Print ASCII */
        print_ascii(data + i, line_bytes);
        putchar('\n');
    }
}
