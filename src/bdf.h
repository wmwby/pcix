/* src/bdf.h */
#ifndef BDF_H
#define BDF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint16_t domain;    /* PCI domain (0 if not specified) */
    uint8_t bus;        /* Bus number */
    uint8_t device;     /* Device number */
    uint8_t function;   /* Function number */
} bdf_t;

/* Parse BDF string into bdf_t structure.
 * Returns true on success, false on invalid format.
 * Supports: "DD:BB:DD.F" and "DDDD:DD:DD.F" formats.
 */
bool bdf_parse(const char *str, bdf_t *bdf);

/* Convert bdf_t to sysfs path string.
 * Caller must provide buffer of at least 64 bytes.
 * Returns false if buffer too small (shouldn't happen with 64 bytes).
 */
bool bdf_to_path(const bdf_t *bdf, char *buf, size_t buflen);

/* Validate that a PCI device exists in sysfs.
 * Returns true if device directory exists.
 */
bool bdf_device_exists(const bdf_t *bdf);

#endif /* BDF_H */
