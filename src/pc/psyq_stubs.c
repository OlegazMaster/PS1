/* The PSY-Q libraries in the PC port: for now each function only says
 * that it was called (the first time) and returns 0. They are replaced one by
 * one by the real layer: graphics, sound, the CD, the pad (docs/roadmap.md,
 * stage 4). Names as the game code uses them; the comments give the PSY-Q
 * function where it is known. */
#include "types.h"

#include <stdio.h>

static void stub(const char *name, int *seen)
{
    if (!*seen) {
        *seen = 1;
        fprintf(stderr, "port: %s is not implemented yet\n", name);
    }
}

#define STUB(name) \
    u32 name() \
    { \
        static int seen; \
        stub(#name, &seen); \
        return 0; \
    }

STUB(InitCARD)
STUB(StartCARD)
STUB(StopCARD)
STUB(_bu_init)
STUB(func_8004592C)
STUB(func_80045E48)
STUB(func_8004B590)
STUB(func_8004BFBC)
STUB(func_8004F9B4)
STUB(func_80050554) /* SpuSetReverb */
STUB(func_800508C4) /* SpuSetReverbModeParam */
STUB(func_80051274) /* SpuSetReverbVoice */
STUB(func_80051714) /* SpuSetKey */
STUB(func_800518D4)
STUB(func_80051934)
STUB(func_80051994) /* Starts the sound: frees the voices, turns the reverb on, sets the volumes. */
STUB(func_80051A74) /* SpuSetCommonAttr */
STUB(func_80051EC8) /* SpuGetAllKeysStatus */
STUB(func_80051F54) /* SpuSetVoiceAttr */
STUB(func_80052844) /* SpuGetVoiceAttr */
STUB(func_80052DD4)
STUB(func_80052F60)
STUB(func_800531E8) /* SsSeqSetCrescendo-like: fades the volume */
STUB(func_8005321C)
STUB(func_800533C8)
STUB(func_80053418) /* SsSepOpen */
STUB(func_80054F70) /* SsSeqStop */
STUB(func_80054FD4) /* SsSeqPlay */
STUB(func_800550D0) /* SsSeqReplay */
STUB(func_80055480)
STUB(func_80056950) /* SsSeqPause */
STUB(func_8005697C)
STUB(func_80056B54)
STUB(func_80056DAC) /* Sets the volume of the music. */
STUB(func_8005AA14)
STUB(func_8005AA28)
STUB(func_8005AA38)
STUB(func_8005AA68) /* Stops the sequence a and (with all) the other one. */
STUB(func_8005AE6C) /* SsVabTransBody */
STUB(func_8005AF3C) /* SsVabOpenHead */
