/* tests/test_memmap.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>

#include "../src/memmap.h"
#include "../src/bdf.h"
#include "../src/resource.h"

/* Test result tracking */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Test helpers */
#define TEST_START(name) \
    do { \
        tests_run++; \
        printf("Running test: %s ... ", name); \
        fflush(stdout); \
    } while(0)

#define TEST_PASS() \
    do { \
        tests_passed++; \
        printf("PASS\n"); \
    } while(0)

#define TEST_FAIL(msg) \
    do { \
        tests_failed++; \
        printf("FAIL: %s\n", msg); \
    } while(0)

#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            TEST_FAIL(msg); \
            return; \
        } \
    } while(0)

#define ASSERT_EQ(a, b, msg) \
    do { \
        if ((a) != (b)) { \
            char buf[256]; \
            snprintf(buf, sizeof(buf), "%s (expected %d, got %d)", msg, (int)(b), (int)(a)); \
            TEST_FAIL(buf); \
            return; \
        } \
    } while(0)

#define ASSERT_NULL(ptr, msg) \
    do { \
        if ((ptr) != NULL) { \
            TEST_FAIL(msg); \
            return; \
        } \
    } while(0)

/* Test 1: memmap_open_bar with NULL bdf */
void test_open_bar_null_bdf(void) {
    TEST_START("open_bar_null_bdf");
    memmap_region_t region;
    int result = memmap_open_bar(NULL, 0, &region);
    ASSERT_EQ(result, -1, "Should return -1 for NULL bdf");
    TEST_PASS();
}

/* Test 2: memmap_open_bar with NULL region */
void test_open_bar_null_region(void) {
    TEST_START("open_bar_null_region");
    bdf_t bdf = {0, 0, 0, 0};
    int result = memmap_open_bar(&bdf, 0, NULL);
    ASSERT_EQ(result, -1, "Should return -1 for NULL region");
    TEST_PASS();
}

/* Test 3: memmap_open_bar with invalid BAR number (negative) */
void test_open_bar_invalid_bar_negative(void) {
    TEST_START("open_bar_invalid_bar_negative");
    bdf_t bdf = {0, 0, 0, 0};
    memmap_region_t region;
    int result = memmap_open_bar(&bdf, -1, &region);
    ASSERT_EQ(result, -1, "Should return -1 for negative BAR number");
    TEST_PASS();
}

/* Test 4: memmap_open_bar with invalid BAR number (>5) */
void test_open_bar_invalid_bar_too_high(void) {
    TEST_START("open_bar_invalid_bar_too_high");
    bdf_t bdf = {0, 0, 0, 0};
    memmap_region_t region;
    int result = memmap_open_bar(&bdf, 6, &region);
    ASSERT_EQ(result, -1, "Should return -1 for BAR number > 5");
    TEST_PASS();
}

/* Test 5: memmap_open_bar with non-existent device */
void test_open_bar_device_not_found(void) {
    TEST_START("open_bar_device_not_found");
    bdf_t bdf = {0, 0xFF, 0xFF, 0};  /* Non-existent device */
    memmap_region_t region;
    int result = memmap_open_bar(&bdf, 0, &region);
    ASSERT_EQ(result, -2, "Should return -2 for device not found");
    TEST_PASS();
}

/* Test 6: memmap_read with NULL region */
void test_read_null_region(void) {
    TEST_START("read_null_region");
    uint8_t buf[4];
    int result = memmap_read(NULL, 0, buf, sizeof(buf));
    ASSERT_EQ(result, -1, "Should return -1 for NULL region");
    TEST_PASS();
}

/* Test 7: memmap_read with NULL buffer */
void test_read_null_buffer(void) {
    TEST_START("read_null_buffer");
    memmap_region_t region = {0};
    int result = memmap_read(&region, 0, NULL, 4);
    ASSERT_EQ(result, -1, "Should return -1 for NULL buffer");
    TEST_PASS();
}

/* Test 8: memmap_close with NULL region (should not crash) */
void test_close_null_region(void) {
    TEST_START("close_null_region");
    memmap_close(NULL);  /* Should not crash */
    TEST_PASS();
}

/* Test 9: memmap_close with zeroed region (should not crash) */
void test_close_zeroed_region(void) {
    TEST_START("close_zeroed_region");
    memmap_region_t region = {0};
    memmap_close(&region);  /* Should not crash */
    TEST_PASS();
}

/* Test 10: memmap_close cleans up properly */
void test_close_cleanup(void) {
    TEST_START("close_cleanup");
    memmap_region_t region = {0};
    region.addr = (void*)0x1234;  /* Fake address */
    region.fd = 42;  /* Fake fd */
    memmap_close(&region);
    ASSERT_NULL(region.addr, "addr should be NULL after close");
    /* fd should be -1 after close */
    TEST_PASS();
}

/* Test 11: Real device with BAR0 (requires root and valid device) */
void test_open_bar_real_device(void) {
    TEST_START("open_bar_real_device");

    /* Skip test if not root */
    if (geteuid() != 0) {
        printf("SKIP (not root)\n");
        tests_run--;  /* Don't count as run */
        return;
    }

    /* Find a real PCI device with BAR0 */
    bdf_t bdf = {0, 0, 0, 0};
    bool found_device = false;

    /* Try common devices: 00:00.0, 00:01.0, 00:02.0 */
    const char *device_paths[] = {"0000:00:00.0", "0000:00:01.0", "0000:00:02.0"};
    for (int i = 0; i < 3; i++) {
        if (bdf_parse(device_paths[i], &bdf)) {
            if (bdf_device_exists(&bdf)) {
                /* Check if BAR0 exists and is enabled */
                resource_list_t resources;
                if (resource_read_all(&bdf, &resources) == 0) {
                    for (int j = 0; j < resources.count; j++) {
                        if (resources.entries[j].bar_num == 0 && resources.entries[j].is_enabled) {
                            found_device = true;
                            break;
                        }
                    }
                }
                if (found_device) break;
            }
        }
    }

    if (!found_device) {
        printf("SKIP (no device with enabled BAR0 found)\n");
        tests_run--;
        return;
    }

    memmap_region_t region;
    int result = memmap_open_bar(&bdf, 0, &region);

    if (result == -4) {
        printf("SKIP (/dev/mem permission denied)\n");
        tests_run--;
        return;
    }

    ASSERT_EQ(result, 0, "Should return 0 on success");
    ASSERT_TRUE(region.addr != NULL, "addr should not be NULL");
    ASSERT_TRUE(region.addr != MAP_FAILED, "addr should not be MAP_FAILED");
    ASSERT_TRUE(region.fd >= 0, "fd should be valid");

    /* Clean up */
    memmap_close(&region);

    TEST_PASS();
}

/* Test 12: memmap_read from real device */
void test_read_from_real_device(void) {
    TEST_START("read_from_real_device");

    /* Skip test if not root */
    if (geteuid() != 0) {
        printf("SKIP (not root)\n");
        tests_run--;
        return;
    }

    /* Find a real PCI device with BAR0 */
    bdf_t bdf = {0, 0, 0, 0};
    bool found_device = false;

    const char *device_paths[] = {"0000:00:00.0", "0000:00:01.0", "0000:00:02.0"};
    for (int i = 0; i < 3; i++) {
        if (bdf_parse(device_paths[i], &bdf)) {
            if (bdf_device_exists(&bdf)) {
                resource_list_t resources;
                if (resource_read_all(&bdf, &resources) == 0) {
                    for (int j = 0; j < resources.count; j++) {
                        if (resources.entries[j].bar_num == 0 && resources.entries[j].is_enabled) {
                            found_device = true;
                            break;
                        }
                    }
                }
                if (found_device) break;
            }
        }
    }

    if (!found_device) {
        printf("SKIP (no device with enabled BAR0 found)\n");
        tests_run--;
        return;
    }

    memmap_region_t region;
    int result = memmap_open_bar(&bdf, 0, &region);

    if (result == -4) {
        printf("SKIP (/dev/mem permission denied)\n");
        tests_run--;
        return;
    }

    if (result != 0) {
        TEST_FAIL("Failed to open BAR");
        return;
    }

    /* Try reading first 4 bytes */
    uint32_t value;
    result = memmap_read(&region, 0, &value, sizeof(value));

    memmap_close(&region);

    ASSERT_EQ(result, 0, "Should return 0 on successful read");

    TEST_PASS();
}

/* Test 13: memmap_read with offset out of bounds */
void test_read_offset_out_of_bounds(void) {
    TEST_START("read_offset_out_of_bounds");

    memmap_region_t region = {0};
    region.size = 4096;

    uint8_t buf[4];
    int result = memmap_read(&region, 5000, buf, sizeof(buf));

    ASSERT_EQ(result, -2, "Should return -2 for offset out of bounds");

    TEST_PASS();
}

/* Test 14: memmap_read with size exceeding region */
void test_read_size_exceeds_region(void) {
    TEST_START("read_size_exceeds_region");

    memmap_region_t region = {0};
    region.size = 100;
    region.addr = (void*)0x1000;  /* Fake address for test */

    uint8_t buf[200];
    int result = memmap_read(&region, 0, buf, sizeof(buf));

    ASSERT_EQ(result, -2, "Should return -2 for size exceeding region");

    TEST_PASS();
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("=== memmap module tests ===\n\n");

    /* Run all tests */
    test_open_bar_null_bdf();
    test_open_bar_null_region();
    test_open_bar_invalid_bar_negative();
    test_open_bar_invalid_bar_too_high();
    test_open_bar_device_not_found();
    test_read_null_region();
    test_read_null_buffer();
    test_close_null_region();
    test_close_zeroed_region();
    test_close_cleanup();
    test_open_bar_real_device();
    test_read_from_real_device();
    test_read_offset_out_of_bounds();
    test_read_size_exceeds_region();

    /* Print summary */
    printf("\n=== Test Summary ===\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);

    if (tests_failed > 0) {
        printf("\nFAILED\n");
        return 1;
    } else if (tests_run == 0) {
        printf("\nNo tests were run!\n");
        return 1;
    }

    printf("\nOK\n");
    return 0;
}
