/* A small interpreter of the R3000 for the code the game loads at run time
 * (the overlay S0/WARPSCUS.BIN: the memory card, the title and warp room
 * extras), which can be neither compiled nor recompiled ahead of time.
 *
 * pc_mips_call(addr, args) runs the code at addr as a C function: the
 * arguments in a0..a3 and on its stack, the result in v0. The code runs from
 * the PlayStation RAM with its delay slots, the R3000 division, lwl/lwr and
 * the GTE (the software GTE). A call out of it to a C function of the game
 * (by its PlayStation address) calls the port's version, a call into nd_asm
 * goes to the recompiled code with all the registers; anything else (the
 * PSY-Q libraries in the executable, for instance) is interpreted too. */
#include "types.h"
#include "gte_soft.h"
#include "nd_recomp.h"

#include <stdio.h>
#include <string.h>

/* src/pc/startup.c: the C function at a PlayStation address (0: none) */
u32 pc_func_lookup(u32 psx);
s32 pc_func_is_native(u32 addr);
extern volatile s32 pc_shadow_orig;
extern u32 pc_shadow_steps;
void pc_shadow_unsafe(const char *what);
u32 nd_exec(u32 pc, RcRegs *regs);

#define ND_LO 0x80039E3Cu
#define ND_HI 0x80045750u
#define MIPS_RETURN 0xFFFFFFF0u
#define PSYQ_LO 0x80045750u /* the PSY-Q libraries in the executable */
#define PSYQ_HI 0x8005BC40u

static u32 stack_area[0x4000]; /* 64 KB for the stacks of the interpreted code */

/* whether an address is in those stacks (the shadow check) */
s32 pc_mips_in_stack(u32 a)
{
    return a - (u32)stack_area < sizeof(stack_area);
}

/* the memory at an address the code uses: kseg0/kseg1/kuseg mirrors of the
 * RAM, the scratchpad where the port has it */
static u8 *mem(u32 a)
{
    if (a - 0x80000000u < 0x200000u || a - (u32)stack_area < sizeof(stack_area)) {
        return (u8 *)a;
    }
    if (a - 0x1F800000u < 0x4000u) {
        return (u8 *)(a - 0x1F800000u + PSX_SCRATCH);
    }
    if (a < 0x200000u || (a & 0xFFE00000u) == 0xA0000000u) {
        return (u8 *)(0x80000000u | (a & 0x1FFFFF)); /* the mirrors of the RAM */
    }
    return (u8 *)a; /* memory of the port (a buffer of the C code: its stack, its heap) */
}

static u32 lw(u32 a)
{
    u32 v;

    memcpy(&v, mem(a), 4);
    return v;
}

static void sw(u32 a, u32 v)
{
    memcpy(mem(a), &v, 4);
}

typedef u32 (*CFunc)(u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32);

static u32 logged;

/* a C function called from the code: the arguments in a0..a3 and on the
 * stack (up to 12: func_80032710 has 9) */
static u32 call_c(u32 f, const u32 *r)
{
    u32 sp = r[29];

    return ((CFunc)f)(r[4], r[5], r[6], r[7], lw(sp + 0x10), lw(sp + 0x14), lw(sp + 0x18), lw(sp + 0x1C),
                      lw(sp + 0x20), lw(sp + 0x24), lw(sp + 0x28), lw(sp + 0x2C));
}

/* the port's libapi (src/pc/libetc.c) */
s32 OpenEvent(u32 desc, s32 spec, s32 mode, void (*func)(void));
s32 CloseEvent(s32 id);
s32 TestEvent(s32 id);
s32 EnableEvent(s32 id);
s32 DisableEvent(s32 id);

/* The BIOS functions the libraries call (table 0xA0, 0xB0 or 0xC0, function
 * t1, arguments a0..a3 in r). The memory card answers as with no card. */
static u32 bios(u32 table, u32 fn, u32 *r)
{
    static u32 seen[3][256 / 32];
    u32 t = table == 0xA0 ? 0 : table == 0xB0 ? 1 : 2;

    if (table == 0xB0) {
        switch (fn) {
        case 0x07: return 1;                                           /* DeliverEvent */
        case 0x08: return (u32)OpenEvent(r[4], (s32)r[5], (s32)r[6], NULL); /* (no handler in the overlay) */
        case 0x09: return (u32)CloseEvent((s32)r[4]);
        case 0x0A: return (u32)TestEvent((s32)r[4]);                     /* WaitEvent */
        case 0x0B: return (u32)TestEvent((s32)r[4]);
        case 0x0C: return (u32)EnableEvent((s32)r[4]);
        case 0x0D: return (u32)DisableEvent((s32)r[4]);
        case 0x4A: case 0x4B: case 0x4C: return 1;                         /* InitCard, StartCard, StopCard */
        case 0x4E: case 0x4F: return 0;                                    /* write/read a card sector: none */
        case 0x50: return 1;                                               /* allow_new_card */
        default: break;
        }
    } else if (table == 0xA0) {
        switch (fn) {
        case 0x17: return (u32)strcmp((const char *)mem(r[4]), (const char *)mem(r[5]));
        case 0x18: return (u32)strncmp((const char *)mem(r[4]), (const char *)mem(r[5]), r[6]);
        case 0x19: strcpy((char *)mem(r[4]), (const char *)mem(r[5])); return r[4];
        case 0x1B: return (u32)strlen((const char *)mem(r[4]));
        case 0x25: return r[4] >= 'a' && r[4] <= 'z' ? r[4] - 32 : r[4]; /* toupper */
        case 0x26: return r[4] >= 'A' && r[4] <= 'Z' ? r[4] + 32 : r[4]; /* tolower */
        case 0x27: memmove(mem(r[5]), mem(r[4]), r[6]); return r[5];      /* bcopy(src, dst, n) */
        case 0x28: memset(mem(r[4]), 0, r[5]); return r[4];               /* bzero */
        case 0x29: return (u32)memcmp(mem(r[4]), mem(r[5]), r[6]);        /* bcmp */
        case 0x2A: memcpy(mem(r[4]), mem(r[5]), r[6]); return r[4];
        case 0x2B: memset(mem(r[4]), (int)r[5], r[6]); return r[4];
        case 0x2C: memmove(mem(r[4]), mem(r[5]), r[6]); return r[4];
        case 0x2D: return (u32)memcmp(mem(r[4]), mem(r[5]), r[6]);
        case 0x2E: {
            u8 *p = memchr(mem(r[4]), (int)(r[5] & 255), r[6]);

            return p ? r[4] + (u32)(p - mem(r[4])) : 0;
        }
        case 0xAB: return 0; /* _card_info: no card (the events report a timeout) */
        case 0xAC: return 0; /* _card_load */
        default: break;
        }
    }
    if (!(seen[t][(fn & 255) / 32] & (1u << (fn & 31)))) {
        seen[t][(fn & 255) / 32] |= 1u << (fn & 31);
        fprintf(stderr, "port: BIOS function %02X:%02X is not implemented (returns 0)\n", table, fn);
    }
    return 0;
}

/* the instruction being interpreted (the diagnostics of the port) */
volatile u32 pc_mips_pc;

/* an address interpreted even when it has a C version (the self-test) */
u32 pc_mips_force;

/* arguments 9 to 12 of the next pc_mips_call (the shadow check) */
u32 pc_mips_extra[4];

static u32 stack_top = 0;      /* the next free word (nested calls) */

/* the depth of the stacks of the interpreted code (the shadow check puts it
 * back when the code it ran crashed) */
u32 pc_mips_stack_top(s32 set, u32 top)
{
    if (set) {
        stack_top = top;
    }
    return stack_top;
}
static u32 warned;

static u32 *regs_io; /* pc_mips_call_regs */

u32 pc_mips_call(u32 addr, u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5, u32 a6, u32 a7)
{
    u32 r[32], hi = 0, lo = 0;
    u32 *io = regs_io;
    u32 pc = addr, npc = addr + 4;
    s32 jumped = 1;
    u32 last_jump = addr;
    u32 base;

    regs_io = NULL;
    memset(r, 0, sizeof(r));
    r[4] = a0;
    r[5] = a1;
    r[6] = a2;
    r[7] = a3;
    /* a stack of its own (0x1000 words per nested call) */
    base = stack_top;
    stack_top += 0x1000;
    if (stack_top > sizeof(stack_area) / 4) {
        fprintf(stderr, "port: the interpreted code nests too deep\n");
        stack_top = base;
        return 0;
    }
    r[29] = (u32)&stack_area[stack_top - 0x20];
    sw(r[29] + 0x10, a4);
    sw(r[29] + 0x14, a5);
    sw(r[29] + 0x18, a6);
    sw(r[29] + 0x1C, a7);
    /* arguments 9 to 12 (the shadow check): once */
    sw(r[29] + 0x20, pc_mips_extra[0]);
    sw(r[29] + 0x24, pc_mips_extra[1]);
    sw(r[29] + 0x28, pc_mips_extra[2]);
    sw(r[29] + 0x2C, pc_mips_extra[3]);
    memset(pc_mips_extra, 0, sizeof(pc_mips_extra));
    r[28] = 0x80060878u; /* $gp of the game */
    r[31] = MIPS_RETURN;
    if (io) {
        /* all the registers from the caller (pc_mips_call_regs) */
        for (int k = 1; k < 32; k++) {
            if (k != 28 && k != 29 && k != 31) {
                r[k] = io[k];
            }
        }
    }

    for (;;) {
        u32 in, op, rs, rt, rd, sa, fn, imm, cur;
        s32 simm;

        /* at the target of a jump (after its delay slot): the exits */
        if (jumped == 1) {
            u32 f;

            jumped = 0;
            if (pc == MIPS_RETURN) {
                break;
            }
            if (pc == pc_mips_force) {
                goto interpret;
            }
            if ((pc & 0x1FFFFFFFu) == 0xA0 || (pc & 0x1FFFFFFFu) == 0xB0 || (pc & 0x1FFFFFFFu) == 0xC0) {
                /* a call of the BIOS: the table at pc, the function in t1 */
                r[2] = bios((pc & 0xFF), r[9], r);
                pc = r[31];
                npc = pc + 4;
                jumped = 1;
                continue;
            }
            if ((pc < 0x80010000u || pc >= 0x80200000u) && pc_func_is_native(pc)) {
                /* a pointer to a C function in the data (patched by the start) */
                r[2] = call_c(pc, r);
                pc = r[31];
                npc = pc + 4;
                jumped = 1;
                continue;
            }
            if (pc < 0x80010000u || pc >= 0x80200000u) {
                if (warned++ < 20) {
                    fprintf(stderr, "port: interpreted code jumps to %08X (from about %08X): returning\n", pc,
                            last_jump);
                }
                break;
            }
            if (pc >= ND_LO && pc < ND_HI) {
                /* the recompiled hand-written assembly, with all registers
                 * (before the C functions: its procedures with register
                 * conventions have C wrappers too) */
                RcRegs regs;
                u32 ra = r[31];

                memcpy(regs.r, r, sizeof(r));
                regs.hi = hi;
                regs.lo = lo;
                regs.ld_reg = regs.ld_old = 0;
                regs.r[31] = RC_RETURN;
                nd_exec(pc, &regs);
                memcpy(r, regs.r, sizeof(r));
                hi = regs.hi;
                lo = regs.lo;
                r[0] = 0;
                pc = ra;
                npc = pc + 4;
                jumped = 1;
                continue;
            }
            if ((f = pc_func_lookup(pc)) == 0 && pc >= PSYQ_LO && pc < PSYQ_HI && logged < 40) {
                logged++;
                fprintf(stderr, "port: interpreting PSY-Q code at %08X (called from %08X)\n", pc, r[31] - 8);
            }
            if (f != 0) {
                /* a C function of the game (or of the port's libraries) */
                r[2] = call_c(f, r);
                pc = r[31];
                npc = pc + 4;
                jumped = 1;
                continue;
            }
        }
    interpret:
        if (pc_shadow_orig && ++pc_shadow_steps > 5000000) {
            pc_shadow_unsafe("too long");
        }
        cur = pc;
        pc_mips_pc = cur;
        in = lw(cur);
        pc = npc;
        npc += 4;
        op = in >> 26;
        rs = (in >> 21) & 31;
        rt = (in >> 16) & 31;
        rd = (in >> 11) & 31;
        sa = (in >> 6) & 31;
        fn = in & 63;
        imm = in & 0xFFFF;
        simm = (s16)imm;

        if (jumped > 1) {
            jumped--; /* set to 2 by a jump: 1 from its delay slot on, checked at the target */
        }
#define BRANCH(c) do { if (c) { npc = cur + 4 + ((u32)simm << 2); jumped = 2; last_jump = cur; } } while (0)
        switch (op) {
        case 0:
            switch (fn) {
            case 0x00: r[rd] = r[rt] << sa; break;
            case 0x02: r[rd] = r[rt] >> sa; break;
            case 0x03: r[rd] = (u32)((s32)r[rt] >> sa); break;
            case 0x04: r[rd] = r[rt] << (r[rs] & 31); break;
            case 0x06: r[rd] = r[rt] >> (r[rs] & 31); break;
            case 0x07: r[rd] = (u32)((s32)r[rt] >> (r[rs] & 31)); break;
            case 0x08: npc = r[rs]; jumped = 2; last_jump = cur; break;
            case 0x09: { u32 t = r[rs]; r[rd] = cur + 8; npc = t; jumped = 2; last_jump = cur; break; }
            case 0x0C: case 0x0D:
                if (warned++ < 5) {
                    fprintf(stderr, "port: syscall/break at %08X in interpreted code\n", cur);
                }
                break;
            case 0x10: r[rd] = hi; break;
            case 0x11: hi = r[rs]; break;
            case 0x12: r[rd] = lo; break;
            case 0x13: lo = r[rs]; break;
            case 0x18: { s64 p = (s64)(s32)r[rs] * (s32)r[rt]; lo = (u32)p; hi = (u32)((u64)p >> 32); break; }
            case 0x19: { u64 p = (u64)r[rs] * r[rt]; lo = (u32)p; hi = (u32)(p >> 32); break; }
            case 0x1A: rc_div(&lo, &hi, r[rs], r[rt]); break;
            case 0x1B: rc_divu(&lo, &hi, r[rs], r[rt]); break;
            case 0x20: case 0x21: r[rd] = r[rs] + r[rt]; break;
            case 0x22: case 0x23: r[rd] = r[rs] - r[rt]; break;
            case 0x24: r[rd] = r[rs] & r[rt]; break;
            case 0x25: r[rd] = r[rs] | r[rt]; break;
            case 0x26: r[rd] = r[rs] ^ r[rt]; break;
            case 0x27: r[rd] = ~(r[rs] | r[rt]); break;
            case 0x2A: r[rd] = (s32)r[rs] < (s32)r[rt]; break;
            case 0x2B: r[rd] = r[rs] < r[rt]; break;
            default: goto bad;
            }
            break;
        case 1: {
            s32 ge = (s32)r[rs] >= 0;

            if (rt & 0x10) {
                r[31] = cur + 8;
            }
            BRANCH((rt & 1) ? ge : !ge);
            break;
        }
        case 2: npc = (cur & 0xF0000000) | ((in & 0x3FFFFFF) << 2); jumped = 2; last_jump = cur; break;
        case 3: r[31] = cur + 8; npc = (cur & 0xF0000000) | ((in & 0x3FFFFFF) << 2); jumped = 2; last_jump = cur; break;
        case 4: BRANCH(r[rs] == r[rt]); break;
        case 5: BRANCH(r[rs] != r[rt]); break;
        case 6: BRANCH((s32)r[rs] <= 0); break;
        case 7: BRANCH((s32)r[rs] > 0); break;
        case 8: case 9: r[rt] = r[rs] + (u32)simm; break;
        case 0x0A: r[rt] = (s32)r[rs] < simm; break;
        case 0x0B: r[rt] = r[rs] < (u32)simm; break;
        case 0x0C: r[rt] = r[rs] & imm; break;
        case 0x0D: r[rt] = r[rs] | imm; break;
        case 0x0E: r[rt] = r[rs] ^ imm; break;
        case 0x0F: /* lui (of the scratchpad: where the port has it, as the C code and nd_asm use it) */
            r[rt] = imm == 0x1F80 ? PSX_SCRATCH : imm << 16;
            break;
        case 0x10:
            /* COP0 (the status of the CPU: interrupts, the GTE on): mfc0
             * reads 0, the rest does nothing */
            if (rs == 0) {
                r[rt] = 0;
            }
            break;
        case 0x12:
            if (in & 0x02000000) {
                gte_command(in & 0x1FFFFFF);
            } else if (rs == 0) {
                r[rt] = gte_read_data(rd);
            } else if (rs == 2) {
                r[rt] = gte_read_ctrl(rd);
            } else if (rs == 4) {
                gte_write_data(rd, r[rt]);
            } else if (rs == 6) {
                gte_write_ctrl(rd, r[rt]);
            }
            break;
        case 0x20: r[rt] = (u32)(s32)(s8)*mem(r[rs] + simm); break;
        case 0x21: { s16 v; memcpy(&v, mem(r[rs] + simm), 2); r[rt] = (u32)(s32)v; break; }
        case 0x22: { u32 a = r[rs] + simm, w = lw(a & ~3u), s = (3 - (a & 3)) * 8;
                     r[rt] = (r[rt] & (0x00FFFFFFu >> (24 - s))) | (w << s); break; }
        case 0x23: r[rt] = lw(r[rs] + simm); break;
        case 0x24: r[rt] = *mem(r[rs] + simm); break;
        case 0x25: { u16 v; memcpy(&v, mem(r[rs] + simm), 2); r[rt] = v; break; }
        case 0x26: { u32 a = r[rs] + simm, w = lw(a & ~3u), s = (a & 3) * 8;
                     r[rt] = (r[rt] & ~(0xFFFFFFFFu >> s)) | (w >> s); break; }
        case 0x28: *mem(r[rs] + simm) = (u8)r[rt]; break;
        case 0x29: { u16 v = (u16)r[rt]; memcpy(mem(r[rs] + simm), &v, 2); break; }
        case 0x2A: { u32 a = r[rs] + simm, w = lw(a & ~3u), s = (3 - (a & 3)) * 8;
                     sw(a & ~3u, (w & ~(0xFFFFFFFFu >> s)) | (r[rt] >> s)); break; }
        case 0x2B: sw(r[rs] + simm, r[rt]); break;
        case 0x2E: { u32 a = r[rs] + simm, w = lw(a & ~3u), s = (a & 3) * 8;
                     sw(a & ~3u, (w & ~(0xFFFFFFFFu << s)) | (r[rt] << s)); break; }
        case 0x32: gte_write_data(rt, lw(r[rs] + simm)); break;
        case 0x3A: sw(r[rs] + simm, gte_read_data(rt)); break;
        default:
        bad:
            if (warned++ < 5) {
                fprintf(stderr, "port: unknown instruction %08X at %08X in interpreted code\n", in, cur);
            }
            break;
        }
        r[0] = 0;
#undef BRANCH
    }
    stack_top = base;
    if (io) {
        memcpy(io, r, sizeof(r));
    }
    return r[2];
}

/* runs the code at addr with the registers regs (but sp, gp and ra), and
 * gives back all of them (the native code of GOOL, src/goolvm.c) */
void pc_mips_call_regs(u32 addr, u32 *regs)
{
    regs_io = regs;
    pc_mips_call(addr, regs[4], regs[5], regs[6], regs[7], 0, 0, 0, 0);
}
