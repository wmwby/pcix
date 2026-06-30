/* src/dump.h */
#ifndef DUMP_H
#define DUMP_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    FORMAT_BYTE,    /* -b: 16 bytes per line */
    FORMAT_WORD,    /* -w: 8 words (16 bytes) per line */
    FORMAT_DWORD    /* -d: 4 dwords (16 bytes) per line */
} dump_format_t;

/* Format and print data in hexdump style.
 * base_offset: starting address to display in left column
 * format: byte, word, or dword display
 */
void dump_print(const uint8_t *data, size_t size,
                uint64_t base_offset, dump_format_t format);

#endif /* DUMP_H */
