/* src/mem_read.h */
#ifndef MEM_READ_H
#define MEM_READ_H

#include "bdf.h"

/* Execute 'mem read' command.
 * argc: number of arguments (after "mem"), argv[0] == "read".
 * Parses -s <bdf>, --bar, --offset, --size, --range, --all, -b/-w/-d.
 * Returns 0 on success, non-zero on error.
 */
int mem_cmd_read(int argc, char *argv[]);

#endif /* MEM_READ_H */
