#include "common.h"
#include "gool.h"
#include "gfx.h"
#include "game.h"
#include "ns.h"
#include "pad.h"

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
void func_80018DD4(s32 *v, MATRIX *m, s32 *out);
s32 GoolObjectChangeState(GoolObject *obj, u32 state, s32 argc, u32 *argv);
extern GoolObject *crash;
extern u32 spawns[];
extern Entry *cur_zone;
extern u16 D_8006AEE8[];
s32 func_80038E28(GoolObject *obj, u32 flags, void *a); /* GoolObjectInterpret */

typedef struct {
    u8 unk0[0x10];
    u32 ticks; /* 0x10 */
} Unk80061A80;
extern Unk80061A80 *D_80061A80;
extern s32 D_80060A98;
extern s16 D_8005D120[];
extern u8 D_80067E40[];
void func_8002D8C8(void);
void func_8001AB64(void);
void func_8002B838(GoolObject *obj, Transform *xform, void *a);
s32 func_8001D674(GoolObject *obj, s32 full);
static inline void SetVec(s32 *v, s32 x, s32 y, s32 z) {
    v[0] = x;
    v[1] = y;
    v[2] = z;
}
/* Updates all objects for a frame (c1: GoolObjectUpdateAll?). */

void func_8001D16C(s32 full) {
    game.frames = D_80061A80->ticks / 34;
    func_8002D8C8();
    switch (crash->state_flags >> 28) {
    case 8:
        SetVec(game.unk384, 0x9C4000, 0x9C4000, 0x9C4000);
        break;
    case 0xD:
        SetVec(game.unk384, 0x1F4000, 0x1F4000, 0x1F4000);
        break;
    default:
        SetVec(game.unk384, 0x96000, 0xAF000, 0x96000);
        break;
    }
    func_8001AB64();
    if (crash != NULL) {
        D_80060A98 = 0;
        GoolForEachHandle(GoolObjectHandleTraverseTreePreorder, (GoolTraverseFunc)func_8001D674, full);
        if (crash->state_flags >> 28 == 0xA || (crash->state_flags >> 28 == 0xE && (game.display_flags & 0x40000000))) {
            func_8002B838(crash, &crash->xform, D_80067E40);
        }
    }
    *(s16 **)(PSX_SCRATCH + 0x005C) = D_8005D120;
}


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

void func_80015798(void);
s32 func_8001CA68(GoolObject *obj, s32 full);
s32 func_8001E78C(GoolObject *obj, s32 *prev, s32 *out);
void func_8001B630(GoolObject *obj, s32 *out);
void func_8001B7A0(GoolObject *obj, s32 *out);
extern s32 paused;
typedef struct {
    s32 v[3];
} Vec3i;
typedef struct {
    Vec3i trans;
    Vec3i rot;
} TransRot;
/* Updates obj for this frame (c1: GoolObjectUpdate): with full set (or
 * state flag 0x40000) and objects of its type enabled, runs the code of its
 * state, resumes it after a wait, updates its light and animation; then
 * draws it if objects of its type are drawn. */

s32 func_8001D674(GoolObject *obj, s32 full) {
    u32 flags;
    u32 sf;
    s32 run;
    s32 res;
    u32 w;
    s32 out[2];
    TransRot prev;

    if (obj == crash) {
        func_80015798();
    }
    flags = game.display_flags;
    game.unk50 = obj;
    if ((run = flags & 8)) {
        sf = obj->state_flags;
        if (sf & 0x40000) {
            full = 1;
        }
        if (((obj->status_b & 0x2000000) || (sf & 0x8000)) && (flags & 0x8000)) {
            run = 1;
        } else {
            switch (GoolObjectType(obj)) {
            case 0x100:
                run = game.display_flags & 0x20;
                break;
            case 0x300:
            case 0x500:
            case 0x600:
                run = game.display_flags & 0x80;
                break;
            case 0x200:
                run = game.display_flags & 0x100;
                break;
            case 0x400:
            case 0x900:
                run = game.display_flags & 0x400;
                break;
            }
        }
    }
    if (full && run) {
        obj->update_stamp = game.frames;
        if (obj->status_b & 0x10) {
            prev.trans = *(Vec3i *)obj->xform.trans;
            prev.rot = *(Vec3i *)obj->xform.rot;
        }
        if (obj->tpc != NULL) {
            func_8001D588(obj, 0, 0xFFFF);
            obj->pc = obj->tpc;
            res = func_80038E28(obj, 3, NULL);
            if (res > -0xFF) {
                return res;
            }
        }
        w = obj->sp[-1];
        if (game.frames - (w & 0xFFFFFF) >= w >> 24 || w >> 24 < 2) {
            obj->sp--;
            res = func_80038E28(obj, 4, NULL);
            if (res > -0xFF) {
                return res;
            }
        }
        func_8001CA68(obj, full);
        func_8001E78C(obj, (s32 *)&prev, out);
        obj->status_a &= ~0x20;
    } else {
        func_8001B630(obj, out);
    }
    if (obj->unkE8 != 0 && (u8)obj->unk120 != 1 && !(obj->status_b & 0x100) && (game.display_flags & 4)) {
        if (((obj->status_b & 0x2000000) || (obj->state_flags & 0x8000)) && (game.display_flags & 0x4000)) {
            run = 1;
        } else {
            run = 0;
            switch (GoolObjectType(obj)) {
            case 0x100:
                if ((game.display_flags & 0x10) && ((game.unk19C & 1) || paused || obj->invincibility_state != 4)) {
                    run = 1;
                }
                break;
            case 0x300:
            case 0x500:
            case 0x600:
                run = game.display_flags & 0x40;
                break;
            case 0x200:
                run = game.display_flags & 0x200;
                if (!full && !(obj->state_flags & 0x40000) && game.pause_obj != 0) {
                    run = 0;
                }
                break;
            case 0x400:
            case 0x900:
                run = game.display_flags & 0x800;
                break;
            }
        }
        if (!full && !(game.display_flags & 0x8000000) && !(obj->state_flags & 0x40000)) {
            run = 0;
        }
        if (run) {
            func_8001B7A0(obj, out);
        }
    }
    game.unk50 = NULL;
    return -0xFF;
}


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

    if (recipient == NULL || recipient->type == 0 || (s32)event >> 8 >= ((GoolHeader *)recipient->global->items[0])->event_count) {
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

s32 *func_8002D0E8(GoolObject *obj, s32 a, s32 idx); /* takes a collision box */
void func_8002D85C(void); /* releases it */
s32 func_8003188C(s32 *point, s32 *box); /* point in box */
s32 func_8003191C(s32 *box_a, s32 *box_b); /* boxes overlap */
s32 func_80031BD0(s32 a, s32 *p1, s32 *p2, s32 rx, s32 ry, s32 rz);

#ifdef TARGET_PC
/* the log of the PC port on the followed objects (src/pc/shadow.c) */
void pc_focus_log(u32 obj, const char *fmt, ...);
#define FOCUS_LOG(obj, ...) pc_focus_log((u32)(obj), __VA_ARGS__)
#else
#define FOCUS_LOG(obj, ...)
#endif

#ifdef NON_MATCHING
/* Sends the event of msg to obj if obj is one of the receivers chosen by
 * msg->type: 1, 4 objects whose box touches the sender's, 2, 3 objects whose
 * box holds the sender, 5 objects in the sender's range (misc_b), 6 objects
 * the sender sweeps through this frame (misc_a); other types send to all.
 * Equivalent; the original swaps the registers of msg and part. */
s32 func_8001DDF4(GoolObject *obj, GoolBroadcast *msg) {
    s32 *box;
    s32 *box2;
    s32 *box3;
    s32 *part;
    s32 hit;
    s32 i;
    s32 j;
    s32 n;
    s32 any;
    s32 pos[3];
    s32 step[3];
    s32 bounds[6];

    if (msg->type != 0) {
        switch (msg->type) {
        case 4:
            if (GoolObjectType(obj) != 0x300 && GoolObjectType(obj) != 0x400) {
                return -0xFF;
            }
        case 1:
            if (obj == msg->sender) {
                return -0xFF;
            }
            if (msg->type == 1) {
                if (!(obj->status_b & 0x10)) {
                    return -0xFF;
                }
                if (msg->sender->collider == obj) {
                    break;
                }
            }
            box = func_8002D0E8(obj, 0, -2);
            if (box == NULL) {
                FOCUS_LOG(obj, "broadcast type %d event %X: no box for the object (records %d)\n", msg->type,
                          msg->event, game.unk5C);
                return -0xFF;
            }
            box2 = func_8002D0E8(msg->sender, 0, -2);
            if (box2 == NULL) {
                FOCUS_LOG(obj, "broadcast type %d event %X: no box for the sender (records %d)\n", msg->type,
                          msg->event, game.unk5C);
                func_8002D85C();
                return -0xFF;
            }
            hit = func_8003191C(box + 2, box2 + 2);
            FOCUS_LOG(obj, "broadcast type %d event %X from %08X: box %X %X %X - %X %X %X, sender %X %X %X - %X %X %X: "
                      "hit %d (records %d)\n", msg->type, msg->event, (u32)msg->sender, box[2], box[3], box[4],
                      box[5], box[6], box[7], box2[2], box2[3], box2[4], box2[5], box2[6], box2[7], hit, game.unk5C);
            func_8002D85C();
            func_8002D85C();
            if (hit) {
                break;
            }
            return -0xFF;
        case 3:
            if (GoolObjectType(obj) != 0x300 && GoolObjectType(obj) != 0x400) {
                return -0xFF;
            }
        case 2:
            box2 = func_8002D0E8(msg->sender, 0, -2);
            if (box2 == NULL) {
                return -0xFF;
            }
            hit = func_8003188C(obj->xform.trans, box2 + 2);
            FOCUS_LOG(obj, "broadcast type %d event %X: position %X %X %X in the sender's box %X %X %X - %X %X %X: %d\n",
                      msg->type, msg->event, obj->xform.trans[0], obj->xform.trans[1], obj->xform.trans[2], box2[2],
                      box2[3], box2[4], box2[5], box2[6], box2[7], hit);
            func_8002D85C();
            if (hit) {
                break;
            }
            return -0xFF;
        case 6:
            if (GoolObjectType(obj) != 0x300) {
                return -0xFF;
            }
            if (func_80031BD0(0, obj->xform.trans, msg->sender->xform.trans, 0xC8000, 0x12C000, 0xC8000)) {
                return -0xFF;
            }
            any = 0;
            if (obj->unkE8 != 0 && *(u8 *)obj->unkE8 == 1) {
                n = *(s32 *)(*(u8 **)((u8 *)obj->unk8 + 0x20) + 0x14);
            } else {
                n = 1;
            }
            step[0] = msg->sender->misc_a[0] / 16;
            step[1] = msg->sender->misc_a[1] / 16;
            step[2] = msg->sender->misc_a[2] / 16;
            for (i = 0; i < n; i++) {
                part = func_8002D0E8(obj, 0, i);
                if (part == NULL) {
                    return -0xFF;
                }
                pos[0] = msg->sender->xform.trans[0];
                pos[1] = msg->sender->xform.trans[1];
                pos[2] = msg->sender->xform.trans[2];
                for (j = 0; j < 16; j++) {
                    any |= func_8003188C(pos, part + 2);
                    pos[0] += step[0];
                    pos[1] += step[1];
                    pos[2] += step[2];
                }
                func_8002D85C();
            }
            FOCUS_LOG(obj, "broadcast type 6 event %X: sweep from %X %X %X by %X %X %X (x16), %d parts: %d\n",
                      msg->event, msg->sender->xform.trans[0], msg->sender->xform.trans[1], msg->sender->xform.trans[2],
                      step[0], step[1], step[2], n, any);
            if (any) {
                break;
            }
            return -0xFF;
        case 5:
            if (GoolObjectType(obj) != 0x300 && GoolObjectType(obj) != 0x400) {
                return -0xFF;
            }
            box3 = func_8002D0E8(obj, 0, -2);
            if (box3 == NULL) {
                return -0xFF;
            }
            bounds[0] = msg->sender->xform.trans[0] - msg->sender->misc_b[0];
            bounds[1] = msg->sender->xform.trans[1] - msg->sender->misc_b[1];
            bounds[2] = msg->sender->xform.trans[2] - msg->sender->misc_b[2];
            bounds[3] = msg->sender->misc_b[0] + msg->sender->xform.trans[0];
            bounds[4] = msg->sender->misc_b[1] + msg->sender->xform.trans[1];
            bounds[5] = msg->sender->misc_b[2] + msg->sender->xform.trans[2];
            hit = func_8003191C(box3 + 2, bounds);
            FOCUS_LOG(obj, "broadcast type 5 event %X: box %X %X %X - %X %X %X, range %X %X %X - %X %X %X: %d\n",
                      msg->event, box3[2], box3[3], box3[4], box3[5], box3[6], box3[7], bounds[0], bounds[1],
                      bounds[2], bounds[3], bounds[4], bounds[5], hit);
            func_8002D85C();
            if (!hit) {
                return -0xFF;
            }
            if (msg->sent >= 3 && msg->sent % 5 != 0) {
                msg->sent++;
                return -0xFF;
            }
            break;
        }
    }
    msg->sent++;
    FOCUS_LOG(obj, "broadcast type %d: event %X sent\n", msg->type, msg->event);
    GoolSendEvent(msg->sender, obj, msg->event, msg->argc, msg->argv);
    if (msg->sender == NULL) {
        return -0xFF;
    }
    if (msg->sender->unkD4 == 0) {
        return -0xFF;
    }
    msg->accepted++;
    if (msg->sender == crash && msg->event == 0x2E00) {
        obj->status_b &= ~0x10;
    }
    return -0xFF;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/interp", func_8001DDF4);
#endif

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

typedef struct {
    u8 unk0[0x18];
    u32 unk18; /* 0x18 */
} Unk80061A7C;
extern Unk80061A7C *D_80061A7C;
s32 func_8001FAC8(s32 angle, s32 target, s32 speed, GoolObject *obj); /* turns angle toward target */
s32 func_80020184(s32 *in, s32 *out);
s32 func_8003D9DC(s32 x); /* square root */
s32 func_8003C2F0(s32 angle); /* sin */
s32 func_8003C364(s32 angle); /* cos */

#ifdef NON_MATCHING
/* Moves obj by the left stick: turns it, accelerates it by params[0] and
 * slows it by params[2] (0xC7 in the air), up to the speed params[1]; sets
 * its speed from the distance to target. Equivalent; the original negates
 * the limit into another register. The return value is not set. */
s32 func_8001E4BC(GoolObject *obj, s32 *params, s32 *target) {
    s32 mag = pads.pads[0].stick_mag;
    s32 dir = pads.pads[0].stick_dir;
    s32 angle = pads.pads[0].stick_angle;
    s32 t;
    s32 f;
    s32 lim;
    s32 v;
    s32 r;
    s32 neg;
    s32 d;
    s32 in[3];
    s32 out[3];

    if (mag < 0x99 || !(obj->status_b & 0x80)) {
        dir = 8;
        mag = 0;
    } else {
        obj->misc_b[1] = func_8001FAC8(obj->misc_b[1], (D_80065C90.unk114 + angle) & 0xFFF, params[5], 0);
    }
    t = D_80061A7C->unk18;
    if ((u32)t > 0x66) {
        t = 0x66;
    }
    mag = (mag * (params[0] * t / 1020)) >> 8;
    in[0] = ((func_8003C2F0(angle) >> 4) * mag) >> 8;
    in[1] = 0;
    in[2] = ((func_8003C364(angle) >> 4) * mag) >> 8;
    func_80020184(in, out);
    obj->misc_a[0] += out[0];
    obj->misc_a[2] += out[2];
    if (dir == 8 || !(obj->status_a & 0x2000000)) {
        f = params[2];
        if (!(obj->status_a & 0x2000000)) {
            f = 0xC7;
        }
        obj->misc_a[0] = (obj->misc_a[0] * f) >> 8;
        obj->misc_a[2] = (obj->misc_a[2] * f) >> 8;
    }
    v = obj->misc_a[0];
    lim = params[1];
    if (v < lim) {
        lim = -lim;
        if (v >= lim) {
            lim = v;
        }
    }
    obj->misc_a[0] = lim;
    v = obj->misc_a[2];
    lim = params[1];
    if (v < lim) {
        lim = -lim;
        if (v >= lim) {
            lim = v;
        }
    }
    obj->misc_a[2] = lim;
    d = func_8003D9DC(((obj->xform.trans[0] - target[0]) >> 6) * ((obj->xform.trans[0] - target[0]) >> 6) + ((obj->xform.trans[2] - target[2]) >> 6) * ((obj->xform.trans[2] - target[2]) >> 6));
    obj->speed = (-(d << 8) + (d << 12)) << 1;
    if (obj->speed < 0x6400) {
        if (dir != 8) {
            obj->speed = 0x6400;
        }
    } else if (params[1] < obj->speed) {
        obj->speed = params[1];
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/interp", func_8001E4BC);
#endif

void func_8001B0BC(GoolObject *obj, s32 *scale);
s32 func_8001FC40(s32 cur, s32 target, s32 speed, GoolObject *obj);
void func_8001FFF0(s32 *in, s32 *pos, s32 *a, s32 *b, s32 unused, s32 *out);
s32 func_80020670(GoolObject *obj, s32 prog, s32 *out);
void func_8002AA94(GoolObject *obj, s32 *delta, void *q);
s32 func_8002D8E0(void);
s32 func_80044CA0(s32 *out, GoolObject *obj, RenderState *rs, s32 *delta);
void func_8005D968(RenderState *rs);
extern s32 D_8005C380[][6]; /* movement parameters (see below) */
extern s32 D_8005C398[6];
extern s32 D_8005C3B0[6];
extern s32 D_8005C3C8[6];
extern s32 D_8005C3E0[6];
extern s32 D_8005C410[6];
extern s32 D_8005C428[6];
extern s32 D_8005C440[6];
extern s32 D_8005C4BC;
extern s32 D_8005C4D0[6];
extern s32 D_8005C530[6];
extern s32 D_8005C560[6];
extern s32 D_8005C590[6];
extern s32 D_8005C5C0[6];
extern s32 D_8006AED8[3];
extern s32 D_8006921C[3];
extern s32 D_80065DA4; /* the camera yaw */
extern RenderState D_800676B0;
/* The time of the frame (in 1/34 s units?), capped. */
#define FRAME_TIME(t) ((t) = D_80061A7C->unk18, (t) >= 0x67 ? (t) = 0x66 : (t))
#define OBJ_W(obj, off) (*(s32 *)((u8 *)(obj) + (off)))
#ifdef NON_MATCHING
/* Moves obj for this frame (c1: GoolObjectPhysics), by its status_b flags:
 * for Crash, the left stick drives its speed and angle with the movement
 * parameters of its state and floor (accel, max speed, friction, stop
 * friction, turn speed); turns its angles toward their targets, applies its
 * velocity (against the zone and the objects with flag 8), follows its path
 * (0x8000), caps the velocity (0x1000), lands on the floor (0x4000) with a
 * bounce in state 0xA, moves the objects standing on it (0x200000), applies
 * gravity (0x20) and collides with Crash (0x10). The return value is
 * not meaningful.
 * Equivalent; differs in registers and scheduling. */
s32 func_8001E78C(GoolObject *obj, s32 *prev, s32 *out) {
    s32 delta[3];
    s32 old[3];
    s32 ret[3];
    s32 path[4];
    s32 *m;
    s32 stick;
    u32 old_flags;
    GoolObject *rider;
    s32 drive;
    s32 a;
    s32 d;
    u32 t;
    s32 v;
    s32 k;
    s32 *r;
    s32 i;
    s32 n;
    u8 *anim;

    if (obj->status_b == 0) {
        obj->status_a &= 0xF8CA207E;
        func_8001B0BC(obj, obj->xform.scale);
        func_8001B630(obj, out);
        obj->collider = NULL;
        return -0xFF;
    }
    drive = obj->status_b & 0x80;
    old_flags = obj->status_a;
    rider = obj->collider;
    if (obj == crash) {
        if ((old_flags & 0x2000000) ||
            ((u32)(game.frames - OBJ_W(obj, 0x170)) < 11 && !(obj->state_flags & 0x282000))) {
            if (!(obj->state_flags & 0x282000)) {
                D_80060924 = D_8005C488;
                func_8001E4BC(obj, (s32 *)D_8005C488, D_8006AED8);
            } else {
                if (!(old_flags & 0x2000000)) {
                    obj->misc_a[0] = (obj->misc_a[0] * 230) >> 8;
                    obj->misc_a[2] = (obj->misc_a[2] * 230) >> 8;
                }
                k = D_8005C4BC;
                if (obj->misc_a[0] < k) {
                    k = -k;
                    if (obj->misc_a[0] >= k) {
                        k = obj->misc_a[0];
                    }
                }
                obj->misc_a[0] = k;
                k = D_8005C4BC;
                if (obj->misc_a[1] < k) {
                    k = -k;
                    if (obj->misc_a[1] >= k) {
                        k = obj->misc_a[1];
                    }
                }
                obj->misc_a[1] = k;
                k = D_8005C4BC;
                if (obj->misc_a[2] < k) {
                    k = -k;
                    if (obj->misc_a[2] >= k) {
                        k = obj->misc_a[2];
                    }
                }
                obj->misc_a[2] = k;
            }
            goto moved;
        }
        switch (obj->state_flags >> 28) {
        case 1:
        case 8:
        case 9:
        case 10:
            drive = 0;
            break;
        }
    }
    if (drive && game.game_state == 0x100) {
        t = obj->state_flags;
        m = D_8005C380[0];
        if (t & 4) {
            if ((t >> 28) == 2) {
                m = D_8005C380[10];
                if (obj->invincibility_state == 5) {
                    m = D_8005C3E0;
                }
            } else if ((t >> 28) == 4) {
                m = D_8005C380[15];
            } else if (t & 0x200000) {
                m = D_8005C380[9];
            } else if (t & 0x80000) {
                if ((t >> 28) == 3 || obj->invincibility_state == 5) {
                    m = D_8005C380[14];
                } else {
                    m = D_8005C380[7];
                }
            } else if (obj->status_a & 0x2000) {
                m = D_8005C380[5];
            } else {
                switch (t >> 28) {
                case 11:
                    m = D_8005C380[19];
                    break;
                case 3:
                    if (obj->invincibility_state == 5) {
                        m = D_8005C380[23];
                    } else {
                        m = D_8005C380[17];
                    }
                    break;
                case 12:
                    m = D_8005C380[21];
                    break;
                default:
                    if (obj->invincibility_state == 5) {
                        m = D_8005C3E0;
                    } else {
                        m = D_8005C398;
                    }
                    break;
                }
            }
        }
        t = obj->state_flags;
        if (t & 8) {
            if (t & 0x80000) {
                if ((t >> 28) == 3 || obj->invincibility_state == 5) {
                    m = D_8005C4D0;
                } else {
                    m = D_8005C440;
                }
            } else if (obj->status_a & 0x2000) {
                m = D_8005C410;
            } else {
                switch (t >> 28) {
                case 3:
                    m = D_8005C530;
                    break;
                case 11:
                    m = D_8005C560;
                    break;
                case 12:
                    m = D_8005C590;
                    break;
                default:
                    if (obj->invincibility_state == 5) {
                        m = D_8005C5C0;
                        break;
                    }
                case 2:
                    m = D_8005C3B0;
                    break;
                }
            }
        }
        if (obj->state_flags & 0x10) {
            m = D_8005C3C8;
        }
        stick = pads.pads[0].stick_mag;
        if (pads.pads[0].stick_dir != 8) {
            a = (D_80065DA4 + pads.pads[0].stick_angle) & 0xFFF;
            d = a - obj->misc_b[1];
            OBJ_W(obj, 0x16C) = a;
            if (d < 0) {
                d = obj->misc_b[1] - a;
            }
            if (d > 0x800) {
                d = 0x1000 - d;
            }
            if (obj->state_flags & 8) {
                v = func_8003C364(d);
                FRAME_TIME(t);
                obj->speed += (((m[0] * (s32)t) / 1020) * (v >> 6)) >> 6;
                if (obj->speed <= 0x100 && !(obj->state_flags & 0x80000)) {
                    obj->misc_b[1] = a;
                } else if (d < 0x7C8 || (obj->state_flags & 0x80000)) {
                    obj->misc_b[1] = func_8001FAC8(obj->misc_b[1], a, m[5], NULL);
                } else {
                    FRAME_TIME(t);
                    obj->speed -= (m[3] * (s32)t) / 1020;
                    if (obj->speed < 0) {
                        obj->speed = 0;
                    }
                }
            } else if (obj->speed <= 0x100 || d <= 0x400) {
                if (obj->state_flags & 0x80000) {
                    obj->misc_b[1] = func_8001FAC8(obj->misc_b[1], a, m[5], NULL);
                } else {
                    obj->misc_b[1] = a;
                }
                FRAME_TIME(t);
                obj->speed += (m[0] * (s32)t) / 1020;
            } else if (m[2] != 0) {
                FRAME_TIME(t);
                obj->speed -= (m[3] * (s32)t) / 1020;
                if (obj->speed < 0) {
                    obj->speed = 0;
                }
            } else {
                obj->speed = 0;
            }
            if (obj->speed > m[1]) {
                obj->speed = m[1];
            }
        } else {
            stick = 0x100;
            if (!(obj->status_a & 0x2000000) && m != D_8005C428 && m != D_8005C428 + 42) {
                FRAME_TIME(t);
                obj->speed -= (m[4] / 1020) * (s32)t;
                if (obj->speed < 0) {
                    obj->speed = 0;
                }
            }
        }
        stick = (stick * obj->speed) >> 8;
        obj->misc_a[0] = ((func_8003C2F0(obj->misc_b[1]) >> 4) * stick) >> 8;
        obj->misc_a[2] = ((func_8003C364(obj->misc_b[1]) >> 4) * stick) >> 8;
    }
moved:
    if (obj == crash) {
        *(Vec3 *)D_8006AED8 = *(Vec3 *)obj->xform.trans;
        for (i = 7; i > 0; i--) {
            ((s32 *)((u8 *)obj->unk8 + 0x4C))[i] = ((s32 *)((u8 *)obj->unk8 + 0x4C))[i - 1];
        }
        *(s32 *)((u8 *)obj->unk8 + 0x4C) = old_flags;
    }
    obj->status_a &= ((obj->status_a & 1) && !(obj->state_flags & 8) ? 0xF8CA0000 : 0xFACA0000) | 0x207E;
    if (obj->status_b & 1) {
        obj->xform.rot[1] = func_8001FAC8(obj->xform.rot[1], obj->misc_b[1], obj->misc_b[0], obj);
    }
    if (obj->status_b & 0x80000) {
        obj->xform.rot[1] = func_8001FC40(obj->xform.rot[1], obj->misc_b[1], obj->misc_b[0], obj);
    }
    if (obj->status_b & 0x2000) {
        obj->xform.rot[0] = func_8001FAC8(obj->xform.rot[0], obj->misc_b[2], *(s32 *)obj->unk124, NULL);
    }
    func_8001B0BC(obj, obj->xform.scale);
    func_8001B630(obj, out);
    obj->collider = NULL;
    if (obj->status_b & 0x40) {
        *(Vec3 *)old = *(Vec3 *)obj->xform.trans;
        FRAME_TIME(t);
        delta[0] = (obj->misc_a[0] * (s32)t) / 1020;
        FRAME_TIME(t);
        delta[1] = (obj->misc_a[1] * (s32)t) / 1020;
        FRAME_TIME(t);
        delta[2] = (obj->misc_a[2] * (s32)t) / 1020;
        if (obj->status_b & 8) {
            obj->event = 0xFF;
            *(s16 **)(PSX_SCRATCH + 0x005C) = D_8005D120;
            if ((obj->state_flags >> 28) == 10) {
                r = *(s32 **)((u8 *)OBJ_W(obj, 0x134) + 0x10);
                *(s32 *)obj->unkFC = func_80044CA0(ret, obj, &D_800676B0, delta);
                *(GoolObject **)D_800676B0.unk348 = crash;
                func_8005D968(&D_800676B0);
                n = ret[0] >> 8;
                if (ret[0] < 0) {
                    n = -ret[0] >> 8;
                }
                k = ret[2];
                if (k < 0) {
                    k = -k;
                }
                k >>= 8;
                r[1] = func_8004F794(ret[0], ret[2]);
                r[0] = 0x400 - func_8004F794(ret[1], func_8003D9DC(n * n + k * k) << 8);
                r[2] = 0;
                if (*(s32 *)obj->unkFC < OBJ_W(obj, 0x13C)) {
                    *(s32 *)obj->unkFC = OBJ_W(obj, 0x13C);
                }
                OBJ_W(obj, 0x13C) = 0x80000000;
                *(s32 *)OBJ_W(obj, 0x134) = *(s32 *)obj->unkFC;
            } else {
                func_8002D8E0();
                func_8002AA94(obj, delta, D_80067E40);
                if ((old_flags & 0x2000000) && !(obj->status_a & 1) && (old_flags & 1)) {
                    obj->status_a |= 0x2000000;
                }
            }
            func_8001AB64();
        } else {
            obj->xform.trans[0] += delta[0];
            obj->xform.trans[1] += delta[1];
            obj->xform.trans[2] += delta[2];
        }
        if (obj->status_b & 0x8000) {
            func_80020670(obj, 0, path);
            *(s32 *)obj->unkFC = path[1];
            if (obj->status_a & 0x200) {
                *(Vec3 *)obj->xform.trans = *(Vec3 *)old;
            }
        }
        if (obj->status_b & 0x1000) {
            k = obj->misc_b[2];
            if (obj->misc_a[0] < k) {
                v = -k;
                if (obj->misc_a[0] >= v) {
                    v = obj->misc_a[0];
                }
            } else {
                v = k;
            }
            obj->misc_a[0] = v;
            if (obj->misc_a[1] < k) {
                v = -k;
                if (obj->misc_a[1] >= v) {
                    v = obj->misc_a[1];
                }
            } else {
                v = k;
            }
            obj->misc_a[1] = v;
            if (obj->misc_a[2] < k) {
                v = -k;
                if (obj->misc_a[2] >= v) {
                    v = obj->misc_a[2];
                }
            } else {
                v = k;
            }
            obj->misc_a[2] = v;
        }
        if ((obj->status_b & 0x4000) && *(s32 *)obj->unkFC >= obj->xform.trans[1]) {
            if ((obj->state_flags >> 28) == 10) {
                FRAME_TIME(t);
                k = (u32)((s32)t * 125 << 12) / 34;
                if (OBJ_W(obj, 0x17C) >= 0 && obj->misc_a[1] < -k && (u32)(obj->update_stamp - obj->unk110) >= 2) {
                    a = ((obj->xform.rot[0] << 20) >> 20) / 4;
                    a = a < 0x71 ? (a < -0x2D ? -0x2D : a) : 0x71;
                    obj->unk114 = obj->misc_a[1];
                    OBJ_W(obj, 0x17C) = (obj->misc_a[1] * (a >= 0 ? a + 0x80 : -a + 0x80)) >> 8;
                    OBJ_W(obj, 0x178) = 0;
                    goto bounce;
                }
                d = *(s32 *)obj->unkFC - obj->xform.trans[1];
                if (d >= (s32)k / 30) {
                bounce:
                    v = OBJ_W(obj, 0x17C) + 0x1D292;
                    obj->status_a |= 1;
                    OBJ_W(obj, 0x17C) = v;
                    obj->unk110 = game.frames;
                    v /= 30;
                    OBJ_W(obj, 0x178) += v;
                    if (OBJ_W(obj, 0x178) > 0) {
                        OBJ_W(obj, 0x178) = 0;
                    }
                    k = obj->xform.trans[1];
                    if (k < *(s32 *)obj->unkFC + OBJ_W(obj, 0x178)) {
                        k = *(s32 *)obj->unkFC + OBJ_W(obj, 0x178);
                    }
                    obj->xform.trans[1] = k;
                    if (OBJ_W(obj, 0x17C) > 0) {
                        obj->misc_a[1] = v * 22;
                        FRAME_TIME(t);
                        obj->misc_a[1] = (u32)(v * 374 * 2) / t;
                    }
                    goto done;
                }
                if ((obj->state_flags >> 28) == 10) {
                    obj->misc_a[1] = d * ((obj->speed * 44) / 2048000);
                    FRAME_TIME(t);
                    OBJ_W(obj, 0x17C) = 0;
                    obj->misc_a[1] = (u32)(obj->misc_a[1] * 34) / t;
                }
            }
            obj->status_a |= 1;
            obj->xform.trans[1] = *(s32 *)obj->unkFC;
            obj->unk110 = game.frames;
            if (obj->misc_a[1] < 0) {
                obj->unk114 = obj->misc_a[1];
                obj->misc_a[1] = 0;
            }
        }
    }
done:
    if ((obj->status_b & 0x200000) && rider != NULL && (old_flags & 0x4000)) {
        delta[0] = obj->xform.trans[0] - prev[0];
        delta[1] = obj->xform.trans[1] - prev[1];
        delta[2] = obj->xform.trans[2] - prev[2];
        rider->xform.trans[0] += delta[0];
        rider->xform.trans[1] += delta[1];
        rider->xform.trans[2] += delta[2];
        delta[0] = rider->xform.trans[0] - obj->xform.trans[0];
        delta[1] = rider->xform.trans[1] - obj->xform.trans[1];
        delta[2] = rider->xform.trans[2] - obj->xform.trans[2];
        old[0] = ((obj->xform.rot[0] - prev[3]) << 20) >> 20;
        old[1] = ((obj->xform.rot[1] - prev[4]) << 20) >> 20;
        old[2] = obj->xform.rot[2];
        if (old[0] != 0 || old[1] != 0) {
            func_8001FFF0(delta, obj->xform.trans, old, NULL, 0, rider->xform.trans);
            rider->misc_b[1] += old[1];
        }
    }
    if (obj->status_b & 0x20) {
        FRAME_TIME(t);
        obj->misc_a[1] -= ((s32)t * 0x3E8000) / 1020;
        if (obj->misc_a[1] < -0x2EE000) {
            obj->misc_a[1] = -0x2EE000;
        }
    }
    if (obj->status_b & 0x10) {
        if (func_80031BD0((s32)obj, obj->xform.trans, crash->xform.trans, D_8006921C[0], D_8006921C[1], D_8006921C[2]) == 0) {
            anim = (u8 *)obj->unkE8;
            n = 1;
            if (anim != NULL && anim[0] == 1) {
                n = *(s32 *)(*(u8 **)((u8 *)obj->unk8 + 0x20) + 0x14);
            }
            for (i = 0; i < n; i++) {
                func_8002D0E8(obj, 1, i);
            }
        }
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/interp", func_8001E78C);
#endif

#define ABS(x) ((x) < 0 ? -(x) : (x))

#ifdef NON_MATCHING
/* Turns the angle cur toward target by speed (scaled by the frame time),
 * slowing down near it when obj has status_b bit 0x20000000. Sets status_a
 * bit 0x800 of obj on reaching target, and bit 8 while turning negatively.
 * Equivalent; the original keeps t in another register and schedules the
 * status stores differently. */
s32 func_8001FAC8(s32 cur, s32 target, s32 speed, GoolObject *obj) {
    s32 t = D_80061A7C->unk18;
    s32 d;
    s32 ad;

    if ((u32)t > 0x66) {
        t = 0x66;
    }
    speed = speed * t / 1020;
    d = ((target - cur) << 20) >> 20;
    ad = ABS(d);
    if (obj != NULL && (obj->status_b & 0x20000000) && ad < speed * 4) {
        if (ad < speed * 2) {
            if (ad < speed) {
                speed /= 8;
            } else {
                speed /= 4;
            }
        } else if (ad < speed * 3) {
            speed /= 2;
        } else {
            speed = speed * 3 / 4;
        }
    }
    if (ad < speed) {
        if (obj != NULL) {
            obj->status_a |= 0x800;
        }
        cur = target;
    } else {
        if (ABS(ad) == 0x800 && target >= 0x800) {
            d = -d;
        }
        if (d < 0) {
            cur = (cur - speed) & 0xFFF;
            if (obj != NULL) {
                obj->status_a |= 8;
            }
        } else {
            cur = (cur + speed) & 0xFFF;
            if (obj != NULL) {
                obj->status_a &= ~8;
            }
        }
    }
    return cur;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/interp", func_8001FAC8);
#endif

#ifdef NON_MATCHING
/* Turns the angle cur toward target by speed (scaled by the frame time);
 * on reaching it sets status_a bit 0x800 of obj. Angles are 12-bit.
 * Equivalent; the original negates the copy ad instead of d. */
s32 func_8001FC40(s32 cur, s32 target, s32 speed, GoolObject *obj) {
    s32 t = D_80061A7C->unk18;
    s32 d;
    s32 ad;

    if ((u32)t > 0x66) {
        t = 0x66;
    }
    speed = speed * t / 1020;
    d = ((target - cur) << 20) >> 20;
    if (d == 0) {
        goto reached;
    }
    ad = d;
    if (d < 0) {
        ad = -ad;
    }
    if (speed >= 0) {
        if (ad >= speed) {
            cur = (cur + speed) & 0xFFF;
            goto end;
        }
    } else if (ad >= -speed) {
        goto move;
    }
    if ((speed ^ d) < 0) {
        goto move;
    }
reached:
    if (obj != NULL) {
        obj->status_a |= 0x800;
    }
    cur = target;
    goto end;
move:
    cur = (cur + speed) & 0xFFF;
end:
    return cur;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/interp", func_8001FC40);
#endif

/* Moves cur toward target by step; returns target once it is within step. */

s32 func_8001FD08(s32 cur, s32 target, s32 step) {
    s32 d = target - cur;

    if (step > 0 && (d < 0 ? -d : d) < step) {
        return target;
    }
    if (d > 0) {
        return cur + step;
    }
    return cur - step;
}

/* Eases x (12-bit) within each period: the position in the period follows a
 * sine curve of amplitude amp (clamped to one). */

s32 func_8001FD58(s32 x, s32 period, s32 amp) {
    s32 sh;
    s32 v;

    x &= 0xFFF;
    sh = (period > 0xFFFF) * 2;
    v = (func_8003C2F0(((x % period) << 11) / period - 0x400) * amp) >> 12;
    v = v >= 0x1000 ? 0x1000 : (v < -0x1000 ? -0x1000 : v);
    v = (((v + 0x1000) >> sh) * period) >> (13 - sh);
    return (x / period) * period + v;
}

void func_8004F544(MATRIX *m, s32 *scale); /* ScaleMatrix */
/* Builds m from the angles rot (y, x, then y relative to z) and the scale
 * (if not NULL). */

void func_8001FE98(MATRIX *m, s32 *rot, s32 *scale) {
    MATRIX r;
    s32 s;
    s32 c;
    s32 s2;
    s32 c2;
    s32 s3;
    s32 c3;

    s = func_8003C2F0(rot[2] & 0xFFF);
    c = func_8003C364(rot[2] & 0xFFF);
    m->m[0][0] = c;
    m->m[0][1] = 0;
    m->m[0][2] = s;
    m->m[1][0] = 0;
    m->m[1][1] = 0x1000;
    m->m[1][2] = 0;
    m->m[2][0] = -s;
    m->m[2][1] = 0;
    m->m[2][2] = c;
    s2 = func_8003C2F0(rot[0]);
    c2 = func_8003C364(rot[0]);
    r.m[0][0] = 0x1000;
    r.m[0][1] = 0;
    r.m[0][2] = 0;
    r.m[1][0] = 0;
    r.m[1][1] = c2;
    r.m[1][2] = -s2;
    r.m[2][0] = 0;
    r.m[2][1] = s2;
    r.m[2][2] = c2;
    MulMatrix(m, &r);
    s3 = func_8003C2F0(-((rot[2] - rot[1]) & 0xFFF));
    c3 = func_8003C364(-((rot[2] - rot[1]) & 0xFFF));
    r.m[0][0] = c3;
    r.m[0][1] = 0;
    r.m[0][2] = s3;
    r.m[1][0] = 0;
    r.m[1][1] = 0x1000;
    r.m[1][2] = 0;
    r.m[2][0] = -s3;
    r.m[2][1] = 0;
    r.m[2][2] = c3;
    MulMatrix(m, &r);
    if (scale != NULL) {
        func_8004F544(m, scale);
    }
}

void func_8001FE98(MATRIX *m, s32 *a, s32 *b); /* builds a matrix */
/* Transforms the point in (in 1/16 units) by the matrix built from a and b
 * and puts it at pos + the result in out. */

void func_8001FFF0(s32 *in, s32 *pos, s32 *a, s32 *b, s32 unused, s32 *out) {
    MATRIX m;
    s32 r[3];
    s32 v[3];

    func_8001FE98(&m, a, b);
    v[0] = in[0] >> 4;
    v[1] = in[1] >> 4;
    v[2] = in[2] >> 4;
    func_80018DD4(v, &m, r);
    r[0] <<= 4;
    r[1] <<= 4;
    r[2] <<= 4;
    out[0] = r[0] + pos[0];
    out[1] = r[1] + pos[1];
    out[2] = r[2] + pos[2];
}

/* Transforms the point in by the model matrix of obj (in 1/16 units) and
 * puts it into world space at out. */

void func_800200C8(GoolObject *obj, s32 unused, s32 *in, s32 *out) {
    MATRIX *m = (MATRIX *)obj->unk8;
    s32 r[3];
    s32 v[3];

    v[0] = in[0] >> 4;
    v[1] = in[1] >> 4;
    v[2] = in[2] >> 4;
    func_80018DD4(v, m, r);
    r[0] <<= 4;
    r[1] <<= 4;
    r[2] <<= 4;
    out[0] = r[0] + obj->xform.trans[0];
    out[1] = r[1] + obj->xform.trans[1];
    out[2] = r[2] + obj->xform.trans[2];
}

/* Rotates the camera-space vector in to world space at out (by the inverse
 * of the camera's roll and yaw). */

s32 func_80020184(s32 *in, s32 *out) {
    MATRIX m;
    MATRIX r;
    s32 s;
    s32 c;
    s32 s2;
    s32 c2;
    Transform *cam = &D_80065C90.unk60;

    s = func_8003C2F0(-cam->rot[2]);
    c = func_8003C364(-cam->rot[2]);
    m.m[0][0] = c;
    m.m[0][1] = -s;
    m.m[0][2] = 0;
    m.m[1][0] = s;
    m.m[1][1] = c;
    m.m[1][2] = 0;
    m.m[2][0] = 0;
    m.m[2][1] = 0;
    m.m[2][2] = 0x1000;
    s2 = func_8003C2F0(-D_80065C90.unk114);
    c2 = func_8003C364(-D_80065C90.unk114);
    r.m[0][0] = c2;
    r.m[0][1] = 0;
    r.m[0][2] = s2;
    r.m[1][0] = 0;
    r.m[1][1] = 0x1000;
    r.m[1][2] = 0;
    r.m[2][0] = -s2;
    r.m[2][1] = 0;
    r.m[2][2] = c2;
    MulMatrix(&m, &r);
    r.m[0][0] = m.m[0][0];
    r.m[0][1] = m.m[1][0];
    r.m[0][2] = m.m[2][0];
    r.m[1][0] = m.m[0][1];
    r.m[1][1] = m.m[1][1];
    r.m[1][2] = m.m[2][1];
    r.m[2][0] = m.m[0][2];
    r.m[2][1] = m.m[1][2];
    r.m[2][2] = m.m[2][2];
    func_80018DD4(in, &r, out);
    return -0xFF;
}

extern MATRIX D_80065C70;
/* Projects the world point in to camera space at out (y up). */

s32 func_800202CC(s32 *in, s32 *out) {
    SVECTOR sv;
    s32 v[3];
    s32 flag;
    s32 *t;

    v[0] = in[0];
    v[1] = in[1];
    v[2] = in[2];
    SetRotMatrix(&D_80065C90.unk20);
    SetTransMatrix(&D_80065C70);
    t = D_80065C90.unk60.trans;
    sv.vx = (v[0] - t[0]) >> 8;
    sv.vy = (v[1] - t[1]) >> 8;
    sv.vz = (v[2] - t[2]) >> 8;
    RotTrans(&sv, (VECTOR *)out, &flag);
    out[0] <<= 8;
    out[1] = -(out[1] << 8);
    out[2] <<= 8;
    return -0xFF;
}

/* Projects the world point in to the screen: out = x, y (from the screen
 * center) and depth. */

s32 func_800203A4(s32 *in, s32 *out) {
    SVECTOR sv;
    VECTOR v;
    s32 sxy;
    s32 p;
    s32 flag;
    s32 sz0;
    s32 sz1;
    s32 sz2;
    s32 *t;

    v.vx = in[0];
    v.vy = in[1];
    v.vz = in[2];
    SetRotMatrix(&D_80065C90.unk20);
    SetTransMatrix(&D_80065C70);
    t = D_80065C90.unk60.trans;
    sv.vx = (v.vx - t[0]) >> 8;
    sv.vy = (v.vy - t[1]) >> 8;
    sv.vz = (v.vz - t[2]) >> 8;
    RotTransPers(&sv, &sxy, &p, &flag);
    out[0] = ((s16)sxy - 0x100) << 8;
    out[1] = (0x6C - (sxy >> 16)) << 8;
    ReadSZfifo3(&sz0, &sz1, &sz2);
    out[2] = sz2 << 8;
    return -0xFF;
}

extern GoolHandle D_80068EA0[8]; /* game.handles, addressed on its own here */
void *malloc(s32 size);
/* Allocates the objects and the player and links all objects into the free
 * list (c1: GoolInitAllocTable). */

s32 func_8002049C(void) {
    s32 i;
    GoolObject *obj;

    crash = NULL;
    game.objects = malloc(96 * 0x23C);
    game.player = malloc(0x33C);
    if (game.objects == NULL || game.player == NULL) {
        return -0xF;
    }
    game.unk50 = NULL;
    game.frames = 0;
    game.player->unk8 = (u8 *)&game + 0xDB0;
    game.free_objects.type = 2;
    game.free_objects.children = game.objects;
    for (i = 0; i < 96; i++) {
        obj = (GoolObject *)((u8 *)game.objects + i * 0x23C);
        obj->unk8 = (u8 *)&game + 0xE1C + i * 0x30;
        obj->type = 0;
        obj->parent = (GoolObject *)&game.free_objects;
        obj->children = NULL;
        if (i < 95) {
            obj->sibling = (GoolObject *)((u8 *)game.objects + (i + 1) * 0x23C);
        } else {
            obj->sibling = NULL;
        }
    }
    for (i = 0; i < 8; i++) {
        D_80068EA0[i].type = 2;
        GoolObjectSetChildren((GoolObject *)&D_80068EA0[i], NULL);
    }
    game.player->type = 0;
    game.player->parent = NULL;
    game.player->children = NULL;
    game.player->sibling = NULL;
    return -0xFF;
}

void free(void *p);
/* Frees the objects and the player (c1: GoolKill). */

void func_800205F4(void) {
    free(game.objects);
    crash = NULL;
    free(game.player);
}

typedef struct {
    u8 unk0[0x4];
    s32 unk4;
    s32 unk8;
} LevelHeader;
/* ns.ldat (D_80061044), seen as its header. */
#define D_80061044 ((LevelHeader *)ns.ldat)

/* Reads the parameters of the level header: game.unk60[0]; fails without
 * unk4. */
s32 func_80020638(void) {
    LevelHeader *h = D_80061044;

    game.unk60[0] = h->unk8 << 8;
    if (h->unk4 == 0) {
        return -0xE;
    }
    return -0xFF;
}

s32 func_80032DF8(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f, s32 def);
s32 func_80032EDC(s32 a, s32 b, s32 c, s32 f);
u8 *func_80032710(u8 *item, s32 tag, s32 pos, s32 index, s32 *outcount, s32 mode, u8 *buf, s32 count, s32 def);
s32 func_8004F794(s32 x, s32 z); /* angle of (x, z) */
void *memset(void *dst, s32 c, s32 n);
#define OBJ_RANGE(obj) (*(s32 *)((u8 *)(obj) + 0x134))
#ifdef NON_MATCHING
/* The point of the path (property 0x4B) of the entity of obj at progress
 * prog (point << 8 | fraction), into out. With status_b 0x8000, follows obj
 * instead: the nearest point of the path (its distance to the side goes to
 * misc_c[1], its progress to unkD4); the angles of obj then face along the
 * path. Returns the number of points (-14 without an entity).
 * Equivalent; the original keeps origin in fp and dx in s7 (swapped here). */
s32 func_80020670(GoolObject *obj, s32 prog, s32 *out) {
    s32 next[3];
    s32 dir[3];
    s32 unused[8]; /* takes stack space in the original */
    s32 near[3];
    u8 buf[4];
    u8 *ent;
    u8 *hdr;
    s32 *origin;
    s32 shift;
    s32 n;
    s32 m;
    s32 idx;
    s32 frac;
    s16 *pt;
    s32 dx;
    s32 dz;
    s32 len;
    s32 t;
    s32 ax;
    s32 az;
    s32 base;
    s32 nx;
    s32 bx;
    s32 bz;
    s32 b2;
    s32 nz;
    u32 old;

    ent = (u8 *)obj->unkF0;
    if (obj == NULL || ent == NULL) {
        return -0xE;
    }
    hdr = *(u8 **)(ent + 4);
    if (*(s32 *)(hdr + 8) == 7) {
        origin = *(s32 **)(hdr + 0x14);
        shift = func_80032DF8((s32)ent, 0x30E, 0, 0, 0, 2, 2);
    }
    n = func_80032EDC((s32)ent, 0x4B, 0, 2);
again:
    idx = prog >> 8;
    if (prog < 0) {
        idx = -prog >> 8;
    }
    m = func_80032EDC((s32)ent, 0x4B, 0, 2);
    if (idx == m - 1 && idx != 0) {
        frac = (prog & 0xFF) + 0x100;
        idx = m - 2;
    } else {
        frac = prog & 0xFF;
    }
    pt = (s16 *)func_80032710(ent, 0x4B, 0, 0, NULL, 2, NULL, 0, 0) + ((idx << 8) >> 8) * 3;
    out[0] = ((pt[0] << shift) + origin[0]) << 8;
    out[1] = ((pt[1] << shift) + origin[1]) << 8;
    out[2] = ((pt[2] << shift) + origin[2]) << 8;
    if (n == 1) {
        return 1;
    }
    pt += 3;
    next[0] = ((pt[0] << shift) + origin[0]) << 8;
    next[1] = ((pt[1] << shift) + origin[1]) << 8;
    next[2] = ((pt[2] << shift) + origin[2]) << 8;
    dir[0] = next[0] - out[0];
    dir[1] = next[1] - out[1];
    dir[2] = next[2] - out[2];
    if (obj->status_b & 0x8000) {
        dx = obj->xform.trans[0] - out[0];
        dz = obj->xform.trans[2] - out[2];
        if ((prog >> 8) < n - 1 && (ABS(dx) > OBJ_RANGE(obj) * 2 || ABS(dz) > OBJ_RANGE(obj) * 2)) {
            prog = (idx + 1) << 8;
            goto again;
        }
        len = func_8003D9DC(((next[0] - out[0]) >> 8) * ((next[0] - out[0]) >> 8) +
                            ((next[2] - out[2]) >> 8) * ((next[2] - out[2]) >> 8));
        if (len != 0) {
            memset(buf, 0, 4);
            buf[0] = 0x80;
            t = ((dx >> 4) * ((next[0] - out[0]) >> 4) + (dz >> 4) * ((next[2] - out[2]) >> 4)) / len;
            frac = t / len;
            if (frac >= 0x100 && (prog >> 8) < n - 1) {
                prog = (idx + 1) << 8;
                goto again;
            }
            near[0] = ((t >> 4) * ((next[0] - out[0]) >> 4)) / len + out[0];
            near[2] = ((t >> 4) * ((next[2] - out[2]) >> 4)) / len + out[2];
            nx = near[0] - obj->xform.trans[0];
            if (nx < 0) {
                nx = obj->xform.trans[0] - near[0];
            }
            nz = near[2] - obj->xform.trans[2];
            if (nz < 0) {
                nz = obj->xform.trans[2] - near[2];
            }
            obj->misc_c[1] = nz < nx ? nx + nz / 2 : nz + nx / 2;
            if (obj->misc_c[1] > OBJ_RANGE(obj) || (frac < 0 && idx == 0) || frac >= 0x100) {
                obj->status_a |= 0x200;
            }
            if ((next[2] - out[2]) * (obj->xform.trans[0] >> 8) - (next[0] - out[0]) * (obj->xform.trans[2] >> 8) -
                    next[2] * (out[0] >> 8) + next[0] * (out[2] >> 8) < 0) {
                obj->misc_c[1] = -obj->misc_c[1];
            }
            base = prog & ~0xFF;
            if (frac < 0xFF) {
                obj->unkD4 = base;
                if (frac >= 0) {
                    obj->unkD4 = base + frac;
                }
            } else {
                obj->unkD4 = base + 0xFF;
            }
        } else {
            obj->status_a |= 0x200;
            b2 = prog & ~0xFF;
            if (frac < 0xFF) {
                obj->unkD4 = b2;
                if (frac >= 0) {
                    obj->unkD4 = b2 + frac;
                }
            } else {
                obj->unkD4 = b2 + 0xFF;
            }
            obj->misc_c[1] = 0;
        }
    }
    old = obj->status_a;
    if (obj->unkF4 < 0 || (prog >> 8) >= n - 1) {
        obj->status_a = old | 4;
    } else {
        obj->status_a = old & ~4;
    }
    if ((obj->status_a & 4) != (old & 4) && !(obj->status_a & 0x10)) {
        obj->status_a |= 0x10;
    } else {
        obj->status_a &= ~0x10;
    }
    if (obj->status_b & 4) {
        if ((obj->status_b & 2) && (obj->status_a & 4)) {
            obj->misc_b[1] = func_8004F794(-dir[0], -dir[2]);
        } else {
            obj->misc_b[1] = func_8004F794(dir[0], dir[2]);
        }
        if (obj->status_b & 0x800) {
            ax = ABS(dir[0]);
            az = ABS(dir[2]);
            obj->xform.rot[2] = func_8004F794(dir[0], dir[2]);
            obj->misc_b[2] = -func_8004F794(dir[1], az < ax ? ax + az / 2 : az + ax / 2);
        }
    } else if (obj->status_b & 0x800) {
        bx = ABS(dir[0]);
        bz = ABS(dir[2]);
        obj->misc_b[2] = func_8004F794(dir[1], bz < bx ? bx + bz / 2 : bz + bx / 2);
        obj->xform.rot[2] = func_8004F794(-dir[0], -dir[2]);
    }
    next[0] = ((next[0] - out[0]) * frac) >> 8;
    next[1] = ((next[1] - out[1]) * frac) >> 8;
    next[2] = ((next[2] - out[2]) * frac) >> 8;
    out[0] += next[0];
    out[1] += next[1];
    out[2] += next[2];
    return n;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/interp", func_80020670);
#endif

/* Sets spawn flag 8 of the entities listed for zone in D_8006AEE8 (each
 * value: zone << 9 | entity). */

void func_80020F04(s32 zone) {
    u16 *p;

    for (p = D_8006AEE8; *p != 0; p++) {
        if (*p >> 9 == zone) {
            spawns[*p & 0x1FF] |= 8;
        }
    }
}

/* Adds the object with pid_flags pid to the list D_8006AEE8 of the level
 * (in the first hole, or at the end), unless the current zone forbids it. */
void func_80020F68(s32 pid) {
    u16 *p = D_8006AEE8;
    u16 *free = NULL;
    u16 key = (pid >> 8) | ((u16)D_80061044->unk8 << 9);

    if (*(u32 *)(((Entry *)D_80065C90.unkCC)->items[0] + 0x2DC) & 0x2000) {
        return;
    }
    for (; *p != 0; p++) {
        if (*p == key) {
            return;
        }
        if (*p == 1) {
            free = p;
        }
    }
    if (free == NULL) {
        free = p;
    }
    *free = key;
}

/* Removes the object with pid_flags pid from the list D_8006AEE8 of the
 * level (values: level << 9 | pid >> 8; 1 marks a hole). */
void func_80021008(s32 pid) {
    u16 *p = D_8006AEE8;
    u16 key = (pid >> 8) | ((u16)D_80061044->unk8 << 9);

    for (; *p != 0; p++) {
        if (*p == key) {
            if (p[1] != 0) {
                *p = 1;
            } else {
                *p = 0;
            }
            return;
        }
    }
}
