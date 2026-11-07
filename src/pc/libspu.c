/* libspu of the PC port: no sound yet (docs/roadmap.md), but the game waits
 * for the transfers to the sound RAM when it pages sound data in. */
#include "types.h"

/* SpuIsTransferCompleted: a transfer is over at once (there is none) */
s32 func_800519C4(s32 wait)
{
    return 1;
}
