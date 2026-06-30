/* tests/test_dump.c */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../src/dump.h"

/* Test basic byte format output */
void test_byte_format(void) {
    printf("Testing FORMAT_BYTE...\n");
    uint8_t data[] = {0x00, 0x01, 0x02, 0x03, 0x10, 0x20, 0x30, 0x40,
                      0x41, 0x42, 0x43, 'A', 'B', 'C', 0x7F, 0xFF};
    dump_print(data, sizeof(data), 0x1000, FORMAT_BYTE);
    printf("\n");
}

/* Test word format output */
void test_word_format(void) {
    printf("Testing FORMAT_WORD...\n");
    uint8_t data[] = {0x00, 0x01, 0x02, 0x03, 0x10, 0x20, 0x30, 0x40,
                      0x41, 0x42, 0x43, 0x44, 0x50, 0x60, 0x70, 0x80};
    dump_print(data, sizeof(data), 0x2000, FORMAT_WORD);
    printf("\n");
}

/* Test dword format output */
void test_dword_format(void) {
    printf("Testing FORMAT_DWORD...\n");
    uint8_t data[] = {0x00, 0x01, 0x02, 0x03, 0x10, 0x20, 0x30, 0x40,
                      0x41, 0x42, 0x43, 0x44, 0x50, 0x60, 0x70, 0x80};
    dump_print(data, sizeof(data), 0x3000, FORMAT_DWORD);
    printf("\n");
}

/* Test partial line (not a full line of data) */
void test_partial_line(void) {
    printf("Testing partial line (not full buffer)...\n");
    uint8_t data[] = {0x01, 0x02, 0x03};
    dump_print(data, sizeof(data), 0x4000, FORMAT_BYTE);
    printf("\n");
}

/* Test ASCII printable characters */
void test_ascii_output(void) {
    printf("Testing ASCII output with printable chars...\n");
    uint8_t data[] = "Hello, World! This is a test.";
    dump_print(data, strlen((char*)data), 0x5000, FORMAT_BYTE);
    printf("\n");
}

int main(void) {
    printf("=== Dump Module Tests ===\n\n");

    test_byte_format();
    test_word_format();
    test_dword_format();
    test_partial_line();
    test_ascii_output();

    printf("=== All tests completed ===\n");
    return 0;
}
