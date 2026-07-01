/* src/main.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bdf.h"
#include "mem.h"
#include "mem_read.h"
#include "mem_write.h"

#define VERSION "0.1.0"

void print_help(void) {
    printf("pcix - PCI device exploration tool v%s\n\n", VERSION);
    printf("Usage:\n");
    printf("  pcix mem show -s <bdf>              Show memory resources for PCI device\n");
    printf("  pcix mem read -s <bdf> --bar <n>    Read data from a PCI device BAR\n");
    printf("  pcix mem write -s <bdf> --bar <n>   Write data to a PCI device BAR\n");
    printf("  pcix -h                             Show this help message\n\n");
    printf("BDF format: BB:DD.F or DDDD:BB:DD.F (e.g., 01:00.0 or 0000:01:00.0)\n");
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

    /* Handle "mem" subcommand */
    if (strcmp(argv[1], "mem") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: Missing mem subcommand (expected 'show', 'read', or 'write')\n\n");
            print_help();
            return 1;
        }

        if (strcmp(argv[2], "show") == 0) {
            if (argc < 5) {
                fprintf(stderr, "Error: Missing arguments for 'mem show' command\n\n");
                print_help();
                return 1;
            }
            if (strcmp(argv[3], "-s") != 0) {
                fprintf(stderr, "Error: Missing '-s' flag for BDF specification\n\n");
                print_help();
                return 1;
            }
            bdf_t bdf;
            if (!bdf_parse(argv[4], &bdf)) {
                fprintf(stderr, "Error: Invalid BDF format '%s'\n\n", argv[4]);
                print_help();
                return 1;
            }
            return mem_cmd_show(&bdf);
        }

        if (strcmp(argv[2], "read") == 0) {
            /* Hand off to mem_cmd_read, which parses -s and all read options.
             * argv+2 points at "read"; argc-2 counts from there. */
            return mem_cmd_read(argc - 2, argv + 2);
        }

        if (strcmp(argv[2], "write") == 0) {
            /* Hand off to mem_cmd_write, which parses -s and all write options.
             * argv+2 points at "write"; argc-2 counts from there. */
            return mem_cmd_write(argc - 2, argv + 2);
        }

        fprintf(stderr, "Error: Unknown mem command '%s'\n\n", argv[2]);
        print_help();
        return 1;
    }

    fprintf(stderr, "Error: Unknown command '%s'\n\n", argv[1]);
    print_help();
    return 1;
}
