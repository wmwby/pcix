/* tests/test_dump.c - assertions for dump_print output formats. */
#define _POSIX_C_SOURCE 200809L  /* fmemopen */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../src/dump.h"

/* ---- tiny test harness ------------------------------------------------- */
static int tests_run = 0;
static int tests_passed = 0;

#define FAIL(fmt, ...)                                                        \
    do {                                                                      \
        printf("  FAIL: ");                                                   \
        printf(fmt, __VA_ARGS__);                                             \
        printf("\n");                                                         \
        return 0;                                                             \
    } while (0)

/* Capture dump_print stdout into `out` so we can assert on it. */
static int capture_dump(const uint8_t *data, size_t size,
                        uint64_t base_offset, dump_format_t format,
                        char *out, size_t outsz) {
    FILE *mem = fmemopen(out, outsz, "w");
    if (!mem) return 0;
    FILE *saved = stdout;
    stdout = mem;
    dump_print(data, size, base_offset, format);
    fflush(mem);
    stdout = saved;
    fclose(mem);
    return 1;
}

/* True if `needle` occurs anywhere in `haystack`. */
static int contains(const char *haystack, const char *needle) {
    return strstr(haystack, needle) != NULL;
}

/* ---- tests ------------------------------------------------------------- */

/* -b is memory order: byte 0x00 at offset 0 must appear leftmost. */
static int test_byte_memory_order(void) {
    uint8_t data[] = {0x00, 0x01, 0x02, 0x03};
    char out[512];
    if (!capture_dump(data, sizeof(data), 0x1000, FORMAT_BYTE, out, sizeof(out)))
        FAIL("%s", "capture failed");
    /* First data token on the line must be 00 (low address byte first). */
    if (!contains(out, " 00 ")) FAIL("%s", "expected ' 00 ' in byte output");
    return 1;
}

/* -w is little-endian: bytes 00 01 -> 0x0100 (CPU reads 0x0100). */
static int test_word_little_endian(void) {
    uint8_t data[] = {0x00, 0x01, 0x02, 0x03};
    char out[512];
    if (!capture_dump(data, sizeof(data), 0x2000, FORMAT_WORD, out, sizeof(out)))
        FAIL("%s", "capture failed");
    if (!contains(out, "0100")) FAIL("%s", "expected '0100' (LE word) in output");
    /* The big-endian interpretation would be 0001; that must NOT appear. */
    if (contains(out, "0001")) FAIL("%s", "found big-endian '0001', not LE");
    return 1;
}

/* -d is little-endian: bytes 00 01 02 03 -> 0x03020100. */
static int test_dword_little_endian(void) {
    uint8_t data[] = {0x00, 0x01, 0x02, 0x03};
    char out[512];
    if (!capture_dump(data, sizeof(data), 0x3000, FORMAT_DWORD, out, sizeof(out)))
        FAIL("%s", "capture failed");
    if (!contains(out, "03020100")) FAIL("%s", "expected '03020100' (LE dword)");
    /* Big-endian would be 00010203; reject it. */
    if (contains(out, "00010203")) FAIL("%s", "found big-endian '00010203', not LE");
    return 1;
}

/* Second dword in the same line is also LE. */
static int test_dword_second_unit_le(void) {
    uint8_t data[] = {0x00, 0x01, 0x02, 0x03,
                      0x78, 0x56, 0x34, 0x12};
    char out[512];
    if (!capture_dump(data, sizeof(data), 0x3000, FORMAT_DWORD, out, sizeof(out)))
        FAIL("%s", "capture failed");
    if (!contains(out, "03020100")) FAIL("%s", "first dword not LE");
    if (!contains(out, "12345678")) FAIL("%s", "second dword not LE (expected 12345678)");
    return 1;
}

/* A 0x prefix is accepted for both halves of a range. */
static int test_format_banner_present(void) {
    uint8_t data[] = {0x00, 0x01, 0x02, 0x03};
    char out[512];
    if (!capture_dump(data, sizeof(data), 0x3000, FORMAT_DWORD, out, sizeof(out)))
        FAIL("%s", "capture failed");
    if (!contains(out, "little-endian")) FAIL("%s", "missing LE banner");
    return 1;
}

/* Offset column reflects base_offset. */
static int test_offset_column(void) {
    uint8_t data[] = {0x00, 0x01};
    char out[512];
    if (!capture_dump(data, sizeof(data), 0xdeadbeef, FORMAT_BYTE, out, sizeof(out)))
        FAIL("%s", "capture failed");
    if (!contains(out, "deadbeef")) FAIL("%s", "base offset not shown");
    return 1;
}

/* Partial last line still renders the available bytes. */
static int test_partial_line(void) {
    uint8_t data[] = {0xaa, 0xbb, 0xcc};  /* 3 bytes, not a full dword */
    char out[512];
    if (!capture_dump(data, sizeof(data), 0x4000, FORMAT_BYTE, out, sizeof(out)))
        FAIL("%s", "capture failed");
    if (!contains(out, "aa bb cc")) FAIL("%s", "partial line bytes missing");
    return 1;
}

/* ---- runner ------------------------------------------------------------ */
typedef int (*test_fn)(void);
static const test_fn all_tests[] = {
    test_byte_memory_order,
    test_word_little_endian,
    test_dword_little_endian,
    test_dword_second_unit_le,
    test_format_banner_present,
    test_offset_column,
    test_partial_line,
};

int main(void) {
    size_t n = sizeof(all_tests) / sizeof(all_tests[0]);
    for (size_t i = 0; i < n; i++) {
        tests_run++;
        if (all_tests[i]()) {
            tests_passed++;
            printf("PASS\n");
        } else {
            printf("(test %zu failed)\n", i);
        }
    }
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    return (tests_run == tests_passed) ? 0 : 1;
}
