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
    s32 argc; /* 0x8 */
    s32 type; /* 0xC: which objects receive it (1-6) */
    s32 sent; /* 0x10 */
    s32 accepted; /* 0x14 */
    u32 *argv; /* 0x18 */
} GoolBroadcast;
s32 func_8001DDF4(GoolObject *obj, GoolBroadcast *msg);
s32 func_8001D588(GoolObject *obj, s32 argc, u32 flags);
s32 GoolObjectChangeState(GoolObject *obj, u32 state, s32 argc, u32 *argv);
extern GoolObject *crash;
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

typedef struct {
    u8 unk0[0x10];
    s32 event_count; /* 0x10 */
} GoolEventHeader;

#ifdef NON_MATCHING
/* Sends event with argc arguments from argv to recipient: runs its event
 * handler, or changes its state by its event table (c1: GoolSendEvent).
 * sender->unkD4 tells whether the event was accepted. Equivalent; the
 * original swaps the registers of sender and event. */
s32 GoolSendEvent(GoolObject *sender, GoolObject *recipient, u32 event, s32 argc, u32 *argv) {
    s32 res = -0x1A;
    u32 state;
    u32 flags;
    u32 blocked;
    u32 sf;
    s32 i;
    s32 out[20];

    if (recipient == NULL || recipient->type == 0 || (s32)event >> 8 >= ((GoolEventHeader *)recipient->global->items[0])->event_count) {
        goto fail;
    }
    if (sender != NULL) {
        sender->unkD4 = 1;
    }
    recipient->interrupter = sender;
    if (recipient->epc != NULL) {
        *recipient->sp++ = event;
        *recipient->sp++ = (u32)argv;
        func_8001D588(recipient, 2, 0xFFFF);
        recipient->pc = recipient->epc;
        res = func_80038E28(recipient, 8, out);
        if (res != -0x1A) {
            if (res > -0xFF) {
                return res;
            }
            state = out[0];
        }
        if (sender != NULL) {
            sender->unkD4 = out[1];
        }
    }
    if (res > -0xFF) {
        state = ((u16 *)recipient->global->items[3])[(s32)event >> 8];
        if (sender != NULL) {
            sender->unkD4 = state != 0xFF;
        }
        if (state & 0x8000) {
            recipient->event = event;
            for (i = 0; i < argc; i++) {
                *recipient->sp++ = argv[i];
            }
            func_8001D588(recipient, argc, 0xFFFF);
            recipient->pc = (u32 *)recipient->global->items[1] + (state & 0x3FFF);
            return func_80038E28(recipient, 3, NULL);
        }
    }
    if (state != 0xFF) {
        flags = recipient->status_c;
        if (event == 0x1800 || event == 0x2600 || event == 0x1900 || event == 0x2500) {
            flags &= ~2;
        }
        sf = ((GoolState *)recipient->global->items[4])[state].flags;
        if (recipient == crash && (u32)(recipient->invincibility_state - 2) < 3) {
            blocked = (flags | 0x402) & sf;
        } else {
            blocked = flags & sf;
        }
        if (!blocked) {
            recipient->event = event;
            if (event == 0x1800 || event == 0x1900) {
                recipient->status_a |= 0x10000;
            }
            GoolObjectChangeState(recipient, state, argc, argv);
            return -0xFF;
        }
    }
    if (state == 0xFF) {
        return -0xFF;
    }
fail:
    if (sender != NULL) {
        sender->unkD4 = 0;
    }
    return -0xFF;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/interp", GoolSendEvent);
#endif

INCLUDE_ASM("asm/us/nonmatchings/interp", func_8001DDF4);

/* Sends event to all objects of the given type (see func_8001DDF4). */

s32 func_8001E34C(GoolObject *sender, u32 event, s32 type, s32 argc, u32 *argv) {
    GoolBroadcast msg;

    msg.sender = sender;
    msg.event = event;
    msg.argc = argc;
    msg.type = type;
    msg.sent = 0;
    msg.accepted = 0;
    msg.argv = argv;
    GoolForEachHandle(GoolObjectHandleTraverseTreePostorder, (GoolTraverseFunc)func_8001DDF4, (s32)&msg);
    if (sender != NULL) {
        sender->unkD4 = msg.accepted;
    }
    return -0xFF;
}

/* Sends event to the objects under recipient of the given type. */

s32 func_8001E3BC(GoolObject *sender, GoolObject *recipient, u32 event, s32 type, s32 argc, u32 *argv) {
    GoolBroadcast msg;

    msg.sender = sender;
    msg.event = event;
    msg.argc = argc;
    msg.type = type;
    msg.sent = 0;
    msg.accepted = 0;
    msg.argv = argv;
    GoolObjectHandleTraverseTreePostorder(recipient, (GoolTraverseFunc)func_8001DDF4, (s32)&msg);
    if (sender != NULL) {
        sender->unkD4 = msg.accepted;
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
