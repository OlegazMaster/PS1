/* libgte of the PC port. Its functions that only compute run as the
 * original code in the interpreter (INTERPRETED in tools/pc/build.py); this
 * is what is not needed in the port. */
#include "types.h"

/* _patch_gte (called by InitGeom): patches the exception handler of the
 * BIOS for the GTE; the port has no BIOS */
void func_8004F914(void)
{
}
