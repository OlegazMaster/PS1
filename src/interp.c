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
s32 GoolObjectChangeState(GoolObject *obj, u32 state, s32 argc, u32 *argv);
extern GoolObject *crash;
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
    *(s16 **)0x1F80005C = D_8005D120;
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
void func_8001E78C(GoolObject *obj, void *prev, s32 *out);
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
        func_8001E78C(obj, &prev, out);
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
                return -0xFF;
            }
            box2 = func_8002D0E8(msg->sender, 0, -2);
            if (box2 == NULL) {
                func_8002D85C();
                return -0xFF;
            }
            hit = func_8003191C(box + 2, box2 + 2);
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
s32 func_8001FAC8(s32 angle, s32 target, s32 speed, s32 a); /* turns angle toward target */
void func_80020184(s32 *in, s32 *out);
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

INCLUDE_ASM("asm/us/nonmatchings/interp", func_8001E78C);

INCLUDE_ASM("asm/us/nonmatchings/interp", func_8001FAC8);

INCLUDE_ASM("asm/us/nonmatchings/interp", func_8001FC40);

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
