/* src/mem.h */
#ifndef MEM_H
#define MEM_H

#include "bdf.h"

/* Execute 'mem show' command.
 * Returns 0 on success, non-zero on error.
 */
int mem_cmd_show(const bdf_t *bdf);

#endif /* MEM_H */
