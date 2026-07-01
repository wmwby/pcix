/* src/mem_write.h */
#ifndef MEM_WRITE_H
#define MEM_WRITE_H

#include "bdf.h"

/* Execute 'mem write' command.
 * argc: number of arguments (after "mem"), argv[0] == "write".
 * Parses -s <bdf>, --bar, --offset, and one of --value/--file/--hex,
 * plus -b/-w/-d (value width), --yes, --no-verify.
 * Returns 0 on success, non-zero on error.
 */
int mem_cmd_write(int argc, char *argv[]);

#endif /* MEM_WRITE_H */
