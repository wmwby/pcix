/* src/main.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERSION "0.1.0"

void print_help(void) {
    printf("pcix - PCI device exploration tool v%s\n\n", VERSION);
    printf("Usage:\n");
    printf("  pcix mem show -s <bdf>    Show memory resources for PCI device\n");
    printf("  pcix -h                   Show this help message\n\n");
    printf("BDF format: DD:BB:DD.F or DDDD:DD:DD.F (e.g., 01:00.0 or 0000:01:00.0)\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_help();
        return 1;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_help();
        return 0;
    }

    fprintf(stderr, "Error: Unknown command '%s'\n\n", argv[1]);
    print_help();
    return 1;
}
