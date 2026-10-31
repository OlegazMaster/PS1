#include "common.h"
#include "gool.h"
#include "gfx.h"
#include "game.h"
#include "ns.h"

/* The GOOL interpreter: state changes, events, opcodes (gool.c in c1). */

extern u8 D_8005C488[];

/* .sdata */
u8 *D_80060924 = D_8005C488;

/* An event sent to many objects (see func_8001DDF4). */
typedef struct {
    GoolObject *sender;
    u32 event; /* 0x4 */
    u32 *argv; /* 0x8 */
    s32 argc; /* 0xC */
    s32 unk10;
    s32 count; /* 0x14 */
    s32 filter; /* 0x18 */
} GoolBroadcast;
s32 func_8001DDF4(GoolObject *obj, GoolBroadcast *msg);
s32 func_8001D588(GoolObject *obj, s32 argc, u32 flags);
s32 func_80038E28(GoolObject *obj, u32 flags, void *a); /* GoolObjectInterpret */

INCLUDE_ASM("asm/us/nonmatchings/interp", func_8001D16C);

typedef struct {
    u8 unk0[0xC];
    s32 stack_size; /* 0xC: initial stack depth in words */
} GoolHeader;

/* Code offset pc of a state as a pointer (see GoolState). */
#define GoolStatePc(obj, pc) \
    ((pc) != 0x3FFF ? (((pc) & 0x4000) ? (u32 *)(obj)->external->items[1] + ((pc) & 0x3FFF) \
                                       : (u32 *)(obj)->global->items[1] + ((pc) & 0x3FFF)) \
                    : NULL)

#ifdef NON_MATCHING
/* Changes the state of obj; argc arguments from argv are passed to the
 * new state (c1: GoolObjectChangeState). Equivalent; the original keeps i
 * in a1 and so schedules the status_a update after the stack pointer. */
s32 GoolObjectChangeState(GoolObject *obj, u32 state, s32 argc, u32 *argv) {
    u32 *once_p = (u32 *)obj->once_p;
    GoolState *s;
    s32 i;

    if (state == 0xFF) {
        return -0xE;
    }
    obj->state = state;
    s = &((GoolState *)obj->global->items[4])[state];
    obj->status_c = s->status_c;
    obj->external = (Entry *)NSOpen((u32 *)obj->unk10->items[2] + s->extern_idx, 1, 0);
    obj->pc = GoolStatePc(obj, s->cpc);
    obj->epc = GoolStatePc(obj, s->epc);
    obj->tpc = GoolStatePc(obj, s->tpc);
    obj->once_p = 0;
    obj->fp = (u32 *)&obj->self;
    obj->sp = (u32 *)((u8 *)obj + (((GoolHeader *)obj->global->items[0])->stack_size * 4 + 0x40));
    obj->status_a |= 0x20020;
    obj->state_flags = s->flags;
    for (i = 0; i < argc; i++) {
        *obj->sp++ = argv[i];
    }
    func_8001D588(obj, argc, 0xFFFF);
    if (once_p != NULL) {
        func_8001D588(obj, 0, 0xFFFF);
        obj->pc = once_p;
        i = func_80038E28(obj, 0x13, NULL);
        if (i > -0xFF) {
            return i;
        }
    } else {
        *obj->sp++ = 0;
    }
    obj->state_stamp = game.frames;
    if (obj->tpc != NULL) {
        if (obj != game.unk50) {
            return -0xFF;
        }
        func_8001D588(obj, 0, 0xFFFF);
        obj->pc = obj->tpc;
        i = func_80038E28(obj, 3, NULL);
        if (i > -0xFF) {
            return i;
        }
    }
    return -0xFF;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/interp", GoolObjectChangeState);
#endif

/* Pushes a frame for a call with argc arguments on the GOOL stack of obj:
 * the flags, the return address and the offsets of the arguments and of
 * the previous frame (c1: GoolObjectPushFrame). */

s32 func_8001D588(GoolObject *obj, s32 argc, u32 flags) {
    u32 *sp = obj->sp;
    u32 *fp = obj->fp;

    obj->fp = sp;
    *obj->sp++ = flags;
    *obj->sp++ = (u32)obj->pc;
    *obj->sp++ = ((u8 *)(sp - argc) - (u8 *)&obj->self) | (((u8 *)fp - (u8 *)&obj->self) << 16);
    return -0xFF;
}

/* Pops the frame of obj and returns to its caller; the low half of *flags
 * gets the flags of the frame (c1: GoolObjectPopFrame). */
s32 func_8001D5E8(GoolObject *obj, u32 *flags) {
    u32 *fp = obj->fp;
    u32 tag;
    u32 prev;
    u32 f;

    obj->sp = fp + 2;
    tag = *obj->sp;
    prev = tag >> 16;
    if (prev == 0) {
        return ERROR_INVALID_RETURN;
    }
    obj->pc = (u32 *)*--obj->sp;
    f = *--obj->sp;
    if (flags != NULL) {
        *flags = (*flags & 0xFFFF0000) | (f & 0xFFFF);
    }
    obj->sp = (u32 *)((tag & 0xFFFF) + ((u32)obj + 0x40));
    obj->fp = (u32 *)(prev + ((u32)obj + 0x40));
    return -0xFF;
}

INCLUDE_ASM("asm/us/nonmatchings/interp", func_8001D674);

INCLUDE_ASM("asm/us/nonmatchings/interp", GoolSendEvent);

INCLUDE_ASM("asm/us/nonmatchings/interp", func_8001DDF4);

/* Sends event to all objects that pass filter. */

s32 func_8001E34C(GoolObject *sender, u32 event, s32 argc, u32 *argv, s32 filter) {
    GoolBroadcast msg;

    msg.sender = sender;
    msg.event = event;
    msg.argv = argv;
    msg.argc = argc;
    msg.unk10 = 0;
    msg.count = 0;
    msg.filter = filter;
    GoolForEachHandle(GoolObjectHandleTraverseTreePostorder, (GoolTraverseFunc)func_8001DDF4, (s32)&msg);
    if (sender != NULL) {
        sender->unkD4 = msg.count;
    }
    return -0xFF;
}

/* Sends event to the objects under recipient that pass filter. */

s32 func_8001E3BC(GoolObject *sender, GoolObject *recipient, u32 event, s32 argc, u32 *argv, s32 filter) {
    GoolBroadcast msg;

    msg.sender = sender;
    msg.event = event;
    msg.argv = argv;
    msg.argc = argc;
    msg.unk10 = 0;
    msg.count = 0;
    msg.filter = filter;
    GoolObjectHandleTraverseTreePostorder(recipient, (GoolTraverseFunc)func_8001DDF4, (s32)&msg);
    if (sender != NULL) {
        sender->unkD4 = msg.count;
    }
    return -0xFF;
}

/* Calls the GOOL subroutine at offset pc of the code of obj with argc
 * arguments from argv. */

s32 func_8001E42C(GoolObject *obj, u32 pc, s32 argc, u32 *argv) {
    s32 i;

    for (i = 0; i < argc; i++) {
        *obj->sp++ = *argv++;
    }
    func_8001D588(obj, argc, 0xFFFF);
    obj->pc = (u32 *)obj->global->items[1] + (pc & 0x3FFF);
    return func_80038E28(obj, 3, NULL);
}

INCLUDE_ASM("asm/us/nonmatchings/interp", func_8001E4BC);

INCLUDE_ASM("asm/us/nonmatchings/interp", func_8001E78C);
