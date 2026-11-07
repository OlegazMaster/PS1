"""Static recompiler: MIPS I assembly of a segment (asm/us/*.s) -> C.

The result is one function, nd_exec(), with a C variable for each register
and a switch on the PlayStation address of every instruction (falling
through from one to the next as the processor does). Branches jump to the
labels directly; jr/jalr (procedure pointers, computed jumps into unrolled
loops, returns) go through the switch, and an address outside the segment is
called as a C function. The semantics of the R3000 are kept: delay slots,
the load delay (a register loaded is still old in the next instruction; if
that instruction writes it too, the load is lost), division by zero. GTE
instructions and memory accesses go through the macros of
include/nd_recomp.h, which decide how they are done (inline COP2 for the
emulator tests, the software GTE and real memory in the port).

The generated C is a working, unreadable translation of the original code: it
lives in build/ and is never committed (it is a copy of the game code).

usage: recomp.py SEGMENT.s OUT.c [--prefix nd] [--visible C files] [--defined C files]
       [--data names] [--port PORT.c]
"""
import argparse
import re
import sys

REG = ["0", "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
       "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"]

LINE = re.compile(r"/\* ([0-9A-F]+) ([0-9A-F]{8}) ([0-9A-F]{8}) \*/\s+(\S+)\s*(.*)")
GLABEL = re.compile(r"^\s*[ag]label\s+(\S+)")


def sx16(v):
    return v - 0x10000 if v & 0x8000 else v


class Insn:
    def __init__(self, addr, word, text):
        self.addr, self.word, self.text = addr, word, text
        self.op = word >> 26
        self.rs = (word >> 21) & 31
        self.rt = (word >> 16) & 31
        self.rd = (word >> 11) & 31
        self.sa = (word >> 6) & 31
        self.fn = word & 63
        self.imm = word & 0xFFFF
        self.simm = sx16(self.imm)
        self.sym = None  # (kind, symbol) for %hi / %lo
        m = re.search(r"%(hi|lo)\(([^)]+)\)", text)
        if m:
            self.sym = (m.group(1), m.group(2))

    # --- classification -------------------------------------------------
    def kind(self):
        op, fn = self.op, self.fn
        if op == 0:
            if fn == 8:
                return "jr"
            if fn == 9:
                return "jalr"
            return "alu"
        if op == 1:
            return "branch"
        if op == 2:
            return "j"
        if op == 3:
            return "jal"
        if op in (4, 5, 6, 7):
            return "branch"
        return "other"

    def is_jump(self):
        return self.kind() in ("jr", "jalr", "branch", "j", "jal")

    def load_target(self):
        """The register a delayed load writes (lb..lwr, mfc2, cfc2), or None."""
        if self.op in (0x20, 0x21, 0x23, 0x24, 0x25):
            return self.rt or None
        if self.op == 0x12 and not (self.word & 0x02000000) and self.rs in (0, 2):
            return self.rt or None
        return None

    def delay_conflict(self, t):
        """Whether this instruction, right after a load of register t, sees
        the load delay: it reads t and gets the old value (lwl/lwr get the
        loaded value forwarded). A write of t alone makes no difference: the
        load is lost, and the register ends up with the value written."""
        if self.op in (0x22, 0x26):
            return t == self.rs
        return t in self.reads()

    def reads(self):
        op, fn = self.op, self.fn
        if op == 0:
            if fn in (0, 2, 3):
                return {self.rt}
            if fn in (8, 0x11, 0x13):
                return {self.rs}
            if fn == 9:
                return {self.rs}
            if fn in (0x0C, 0x0D, 0x10, 0x12):
                return set()
            return {self.rs, self.rt}
        if op == 1:
            return {self.rs}
        if op in (2, 3):
            return set()
        if op in (4, 5):
            return {self.rs, self.rt}
        if op in (6, 7):
            return {self.rs}
        if op == 0x0F:
            return set()
        if op in (0x22, 0x26):  # lwl/lwr merge with the old value
            return {self.rs, self.rt}
        if 0x20 <= op <= 0x25:
            return {self.rs}
        if 0x28 <= op <= 0x2E:
            return {self.rs, self.rt}
        if op in (0x32, 0x3A):
            return {self.rs}
        if op == 0x12:
            if self.word & 0x02000000:
                return set()
            return {self.rt} if self.rs in (4, 6) else set()
        return {self.rs}

    def writes(self):
        op, fn = self.op, self.fn
        if op == 0:
            if fn == 8 or fn in (0x0C, 0x0D, 0x11, 0x13, 0x18, 0x19, 0x1A, 0x1B):
                return set()
            return {self.rd} - {0}
        if op == 1:
            return {31} if self.rt in (0x10, 0x11) else set()
        if op == 3:
            return {31}
        if op in (2, 4, 5, 6, 7) or 0x28 <= op <= 0x2E or op in (0x32, 0x3A):
            return set()
        if op == 0x12:
            if self.word & 0x02000000:
                return set()
            return {self.rt} - {0} if self.rs in (0, 2) else set()
        return {self.rt} - {0}


def parse(path):
    insns, entries = [], []
    for line in open(path):
        g = GLABEL.match(line)
        if g:
            entries.append(g.group(1))
            continue
        m = LINE.search(line)
        if not m:
            continue
        addr = int(m.group(2), 16)
        b = bytes.fromhex(m.group(3))
        word = int.from_bytes(b, "little")
        insns.append(Insn(addr, word, m.group(4) + " " + re.sub(r"\s*/\*.*", "", m.group(5))))
        if entries and isinstance(entries[-1], str):
            entries[-1] = (entries[-1], addr)
    return insns, entries


class Gen:
    def __init__(self, insns, lo, hi):
        self.insns = insns
        self.by_addr = {i.addr: i for i in insns}
        self.lo, self.hi = lo, hi  # the segment [lo, hi)
        self.c_lo, self.c_hi = lo, hi  # the chunk being generated
        self.labels = set()
        self.syms = set()
        self.hazards = []
        self.pending_regs = set()
        self.visible = set()  # symbols of the segment the C code uses

    def r(self, n):
        return "0u" if n == 0 else "r%d" % n

    def w(self, n, expr):
        if n == 0:
            return "(void)(%s);" % expr
        return "r%d = %s;" % (n, expr)

    def imm_expr(self, i, signed):
        """The immediate of i: symbolic for %lo of a symbol outside the segment."""
        if i.sym and i.sym[0] == "lo" and not self.inside(i.sym[1]):
            self.syms.add(i.sym[1])
            return "RC_LO(%s)" % i.sym[1]
        return "0x%Xu" % (i.simm & 0xFFFFFFFF) if signed else "0x%Xu" % i.imm

    def in_segment(self, sym):
        """The address of a symbol of the segment, or None."""
        m = re.match(r"^(?:\.L|D_|func_|jtbl_|nd_entry_)([0-9A-Fa-f]{8})$", sym)
        if m and self.lo <= int(m.group(1), 16) < self.hi:
            return int(m.group(1), 16)
        return None

    def inside(self, sym):
        """Whether %hi/%lo of sym is emitted as the number: the addresses of
        the segment, except those the C code of the game sees (procedure
        pointers it stores and compares: in the port they are the addresses of
        C wrappers, which the dispatcher maps back)."""
        return self.in_segment(sym) is not None and sym not in self.visible

    def addr(self, i):
        return "(%s + %s)" % (self.r(i.rs), self.imm_expr(i, True))

    def body(self, i):
        """C for an instruction without control flow."""
        op, fn, rs, rt, rd, sa = i.op, i.fn, self.r(i.rs), self.r(i.rt), i.rd, i.sa
        if i.word == 0:
            return ""
        if op == 0:
            S = {0: "%s << %d" % (rt, sa), 2: "%s >> %d" % (rt, sa), 3: "(u32)((s32)%s >> %d)" % (rt, sa),
                 4: "%s << (%s & 31)" % (rt, rs), 6: "%s >> (%s & 31)" % (rt, rs),
                 7: "(u32)((s32)%s >> (%s & 31))" % (rt, rs),
                 0x20: "%s + %s" % (rs, rt), 0x21: "%s + %s" % (rs, rt), 0x22: "%s - %s" % (rs, rt),
                 0x23: "%s - %s" % (rs, rt), 0x24: "%s & %s" % (rs, rt), 0x25: "%s | %s" % (rs, rt),
                 0x26: "%s ^ %s" % (rs, rt), 0x27: "~(%s | %s)" % (rs, rt),
                 0x2A: "(u32)((s32)%s < (s32)%s)" % (rs, rt), 0x2B: "(u32)(%s < %s)" % (rs, rt)}
            if fn in S:
                return self.w(rd, S[fn])
            if fn == 0x10:
                return self.w(rd, "hi")
            if fn == 0x12:
                return self.w(rd, "lo")
            if fn == 0x11:
                return "hi = %s;" % rs
            if fn == 0x13:
                return "lo = %s;" % rs
            if fn == 0x18:
                return "{ s64 p_ = (s64)(s32)%s * (s32)%s; lo = (u32)p_; hi = (u32)((u64)p_ >> 32); }" % (rs, rt)
            if fn == 0x19:
                return "{ u64 p_ = (u64)%s * %s; lo = (u32)p_; hi = (u32)(p_ >> 32); }" % (rs, rt)
            if fn == 0x1A:
                return "rc_div(&lo, &hi, %s, %s);" % (rs, rt)
            if fn == 0x1B:
                return "rc_divu(&lo, &hi, %s, %s);" % (rs, rt)
            if fn == 0x0D:
                return "RC_BREAK(0x%X, %d);" % (i.addr, (i.word >> 6) & 0xFFFFF)
            if fn == 0x0C:
                return "RC_SYSCALL(0x%X);" % i.addr
        if op == 0x0F:
            if i.sym and i.sym[0] == "hi" and not self.inside(i.sym[1]):
                self.syms.add(i.sym[1])
                return self.w(i.rt, "RC_HI(%s)" % i.sym[1])
            if i.imm == 0x1F80:  # the scratchpad and the I/O ports: wherever the port has them
                return self.w(i.rt, "RC_SCRATCH_HI")
            return self.w(i.rt, "0x%Xu" % (i.imm << 16))
        if op in (8, 9):
            return self.w(i.rt, "%s + %s" % (rs, self.imm_expr(i, True)))
        if op == 0x0A:
            return self.w(i.rt, "(u32)((s32)%s < (s32)%s)" % (rs, self.imm_expr(i, True)))
        if op == 0x0B:
            return self.w(i.rt, "(u32)(%s < %s)" % (rs, self.imm_expr(i, True)))
        if op == 0x0C:
            return self.w(i.rt, "%s & 0x%Xu" % (rs, i.imm))
        if op == 0x0D:
            return self.w(i.rt, "%s | %s" % (rs, self.imm_expr(i, False)))
        if op == 0x0E:
            return self.w(i.rt, "%s ^ 0x%Xu" % (rs, i.imm))
        a = self.addr(i)
        L = {0x20: "(u32)(s32)RC_LB(%s)", 0x21: "(u32)(s32)RC_LH(%s)", 0x23: "RC_LW(%s)",
             0x24: "(u32)RC_LBU(%s)", 0x25: "(u32)RC_LHU(%s)"}
        if op in L:
            return self.w(i.rt, L[op] % a)
        if op == 0x22:
            return self.w(i.rt, "rc_lwl(%s, %s)" % (a, rt))
        if op == 0x26:
            return self.w(i.rt, "rc_lwr(%s, %s)" % (a, rt))
        if op == 0x28:
            return "RC_SB(%s, %s);" % (a, rt)
        if op == 0x29:
            return "RC_SH(%s, %s);" % (a, rt)
        if op == 0x2B:
            return "RC_SW(%s, %s);" % (a, rt)
        if op == 0x2A:
            return "rc_swl(%s, %s);" % (a, rt)
        if op == 0x2E:
            return "rc_swr(%s, %s);" % (a, rt)
        if op == 0x32:
            return "RC_MTC2(%d, RC_LW(%s));" % (i.rt, a)
        if op == 0x3A:
            return "{ u32 g_; RC_MFC2(%d, g_); RC_SW(%s, g_); }" % (i.rt, a)
        if op == 0x12:
            if i.word & 0x02000000:
                return "RC_COP2(0x%07X);" % (i.word & 0x1FFFFFF)
            if i.rs == 0:
                return "{ u32 g_; RC_MFC2(%d, g_); %s }" % (rd, self.w(i.rt, "g_"))
            if i.rs == 2:
                return "{ u32 g_; RC_CFC2(%d, g_); %s }" % (rd, self.w(i.rt, "g_"))
            if i.rs == 4:
                return "RC_MTC2(%d, %s);" % (rd, rt)
            if i.rs == 6:
                return "RC_CTC2(%d, %s);" % (rd, rt)
        return "RC_BAD(0x%X, 0x%08X);" % (i.addr, i.word)

    def target(self, i):
        if i.kind() in ("j", "jal"):
            return (i.addr & 0xF0000000) | ((i.word & 0x3FFFFFF) << 2)
        return (i.addr + 4 + (i.simm << 2)) & 0xFFFFFFFF

    def cond(self, i):
        rs, rt = self.r(i.rs), self.r(i.rt)
        if i.op == 4:
            return "%s == %s" % (rs, rt)
        if i.op == 5:
            return "%s != %s" % (rs, rt)
        if i.op == 6:
            return "(s32)%s <= 0" % rs
        if i.op == 7:
            return "(s32)%s > 0" % rs
        if i.rt & 1:
            return "(s32)%s >= 0" % rs
        return "(s32)%s < 0" % rs

    def goto(self, addr):
        if self.c_lo <= addr < self.c_hi and addr in self.by_addr:
            self.labels.add(addr)
            return "goto L_%08X;" % addr
        return "{ pc = 0x%08Xu; goto leave; }" % addr

    def control(self, i, delay, pre=""):
        """C for a jump or branch i with its delay slot instruction (pre: C
        run after the jump has read its registers, before the delay slot)."""
        k = i.kind()
        d = pre + (self.body(delay) if delay else "")
        nxt = i.addr + 8
        if k == "branch":
            link = "r31 = 0x%08Xu; " % nxt if i.op == 1 and i.rt in (0x10, 0x11) else ""
            if d:
                return "{ int c_ = %s; %s%s if (c_) %s } %s" % (self.cond(i), link, d, self.goto(self.target(i)), self.goto(nxt))
            return "%sif (%s) %s %s" % (link, self.cond(i), self.goto(self.target(i)), self.goto(nxt))
        if k == "j":
            return "%s %s" % (d, self.goto(self.target(i)))
        if k == "jal":
            self.labels.add(nxt)
            return "r31 = 0x%08Xu; %s %s" % (nxt, d, self.goto(self.target(i)))
        t = delay.load_target() if delay else None
        if t and k in ("jr", "jalr"):
            # the instruction at the target still sees the old value: the
            # dispatcher handles it (ld_reg, ld_old)
            self.pending_regs.add(t)
            d = "ld_old = r%d; %s ld_reg = %d;" % (t, d, t)
        if k == "jr":
            return "{ u32 t_ = %s; %s pc = t_; goto dispatch; }" % (self.r(i.rs), d)
        if k == "jalr":
            self.labels.add(nxt)
            link = self.w(i.rd, "0x%08Xu" % nxt) if i.rd else ""
            return "{ u32 t_ = %s; %s %s pc = t_; goto dispatch; }" % (self.r(i.rs), link, d)
        raise ValueError(k)

    def code(self, idx):
        """C for the instruction at index idx when execution arrives at it
        (and, for a jump, its delay slot; for a load, the delay)."""
        i = self.insns[idx]
        nxt = self.insns[idx + 1] if idx + 1 < len(self.insns) else None
        if i.is_jump():
            if nxt and nxt.is_jump():
                raise ValueError("jump in a delay slot at %08X" % i.addr)
            t = nxt.load_target() if nxt else None
            if t:
                # a load in a delay slot: the instruction after the jump
                # would see the old value
                k = i.kind()
                after = []
                if k in ("branch", "j", "jal"):
                    after.append(self.target(i))
                always = (i.op == 4 and i.rs == i.rt) or (i.op == 1 and i.rs == 0 and i.rt & 1)
                if k == "branch" and not always:
                    after.append(i.addr + 8)
                for a in after:
                    n = self.by_addr.get(a)
                    if n and n.delay_conflict(t):
                        raise ValueError("load delay after the jump at %08X" % i.addr)
            return self.control(i, nxt)
        t = i.load_target()
        prev = self.insns[idx - 1] if idx else None
        if prev and prev.is_jump():
            t = None  # in a delay slot: control() handles it
        if t and nxt and nxt.delay_conflict(t):
            if nxt.is_jump() or nxt.load_target():
                raise ValueError("unsupported load delay at %08X" % i.addr)
            self.hazards.append((i.addr, i.text.strip() + " | " + nxt.text.strip()))
            ld = self.body(i).replace("r%d = " % t, "u32 ld_ = ", 1)
            tail = "" if t in nxt.writes() else " r%d = ld_;" % t  # a write cancels the load
            return "{ %s %s%s } %s" % (ld, self.body(nxt), tail, self.goto(nxt.addr + 4))
        return self.body(i)

    def pending_cases(self):
        """(address, registers): the places a jr/jalr can reach whose
        instruction reads a register loaded in the delay slot of one (return
        points, entries, code addresses taken with %lo)."""
        cands = set(a for _, a in self.entries)
        for i in self.insns:
            if i.kind() in ("jal", "jalr"):
                cands.add(i.addr + 8)
            if i.sym and i.sym[0] == "lo" and self.in_segment(i.sym[1]) is not None:
                cands.add(self.in_segment(i.sym[1]))
        out = []
        for a in sorted(cands):
            n = self.by_addr.get(a)
            if not n:
                continue
            regs = [t for t in sorted(self.pending_regs) if n.delay_conflict(t)]
            if regs:
                after = self.by_addr.get(a + 4)
                if n.load_target() and after and after.delay_conflict(n.load_target()):
                    raise ValueError("unsupported load delay at the target %08X" % a)
                out.append((a, regs))
        return out

    def chunks(self, size=600):
        """Splits the segment into functions of about size instructions, at
        entries (a compiler takes very long over one function of the whole
        segment)."""
        starts = sorted(a for _, a in self.entries)
        bounds = [self.lo]
        for a in starts:
            prev = self.by_addr.get(a - 4)
            if (a - bounds[-1]) // 4 >= size and not (prev and prev.is_jump()):
                bounds.append(a)
        bounds.append(self.hi)
        return list(zip(bounds, bounds[1:]))

    def emit(self, out, entries, prefix):
        self.entries = entries
        regs_in = "    u32 " + ", ".join("r%d = regs->r[%d]" % (n, n) for n in range(1, 32)) + ";"
        regs_out = "    " + " ".join("regs->r[%d] = r%d;" % (n, n) for n in range(1, 32))
        o = []
        o.append("/* Generated by tools/recomp.py from the original assembly: do not edit, do not commit. */")
        o.append('#include "nd_recomp.h"\n')
        o.append("u32 %s_native_lookup(u32 addr);" % prefix)
        body = []
        chunks = self.chunks()
        # the registers loaded in the delay slots of jr/jalr (control() adds
        # them too, but the targets are needed before the code)
        for i, n in zip(self.insns, self.insns[1:]):
            if i.kind() in ("jr", "jalr") and n.load_target():
                self.pending_regs.add(n.load_target())
        all_pending = self.pending_cases()
        for k, (c_lo, c_hi) in enumerate(chunks):
            self.c_lo, self.c_hi = c_lo, c_hi
            lines = []
            for idx, i in enumerate(self.insns):
                if c_lo <= i.addr < c_hi:
                    c = self.code(idx)
                    lines.append("    case 0x%08X: L_%08X: RC_STEP(0x%08Xu); %s /* %s */"
                                 % (i.addr, i.addr, i.addr, c, i.text.strip()))
            f = []
            f.append("/* 0x%08X-0x%08X */" % (c_lo, c_hi))
            f.append("static u32 %s_c%d(u32 pc, RcRegs *regs)\n{" % (prefix, k))
            f.append(regs_in)
            f.append("    u32 hi = regs->hi, lo = regs->lo;")
            f.append("    u32 ld_reg = regs->ld_reg, ld_old = regs->ld_old;")
            f.append("dispatch:")
            f.append("    if (ld_reg) {")
            f.append("        /* a load in the delay slot of jr/jalr: the instruction at the")
            f.append("           target reads the old value of the register */")
            f.append("        u32 lr_ = ld_reg;")
            f.append("        (void)lr_;")
            f.append("        ld_reg = 0;")
            f.append("        switch (pc) {")
            f.append("        default:")
            f.append("            if (pc - 0x%08Xu >= 0x%Xu) {" % (c_lo, c_hi - c_lo))
            f.append("                ld_reg = lr_; /* for the chunk of the target */")
            f.append("            }")
            f.append("            break;")
            for a, regs in all_pending:
                if not c_lo <= a < c_hi:
                    continue
                n = self.by_addr[a]
                f.append("        case 0x%08X:" % a)
                for t in regs:
                    if n.is_jump():
                        # the jump reads the old value, its delay slot the new one
                        code = self.control(n, self.by_addr.get(a + 4), "r%d = new_; " % t)
                        f.append("            if (lr_ == %d) { u32 new_ = r%d; r%d = ld_old; %s } /* %s */"
                                 % (t, t, t, code, n.text.strip()))
                        continue
                    tail = "" if t in n.writes() else " r%d = new_;" % t
                    f.append("            if (lr_ == %d) { u32 new_ = r%d; r%d = ld_old; %s%s %s } /* %s */"
                             % (t, t, t, self.body(n), tail, self.goto(a + 4), n.text.strip()))
                f.append("            break;")
            f.append("        }")
            f.append("    }")
            f.append("    switch (pc) {")
            f.append("    default:")
            f.append("        goto leave;")
            f.extend(lines)
            f.append("        ;")
            f.append("    }")
            f.append("    pc = 0x%08Xu;" % c_hi)
            f.append("leave:")
            f.append(regs_out)
            f.append("    regs->hi = hi; regs->lo = lo;")
            f.append("    regs->ld_reg = ld_reg; regs->ld_old = ld_old;")
            f.append("    return pc;")
            f.append("}\n")
            body.extend(f)
        for s_ in sorted(self.syms):
            o.append("extern char %s[];" % s_)
        o.append("")
        o.extend(body)
        o.append("static u32 (*const %s_chunks[])(u32, RcRegs *) = {" % prefix)
        o.append("    " + ", ".join("%s_c%d" % (prefix, k) for k in range(len(chunks))))
        o.append("};")
        o.append("static const u32 %s_bounds[] = {" % prefix)
        o.append("    " + ", ".join("0x%08Xu" % b for _, b in chunks))
        o.append("};\n")
        o.append("/* runs the segment from pc with the registers in regs until it")
        o.append("   returns to RC_RETURN; jumps out of it call C functions */")
        o.append("u32 %s_exec(u32 pc, RcRegs *regs)\n{" % prefix)
        o.append("    for (;;) {")
        o.append("        if (pc - 0x%08Xu < 0x%Xu && !(pc & 3)) {" % (self.lo, self.hi - self.lo))
        o.append("            u32 k = 0;\n")
        o.append("            while (pc >= %s_bounds[k]) {" % prefix)
        o.append("                k++;")
        o.append("            }")
        o.append("            pc = %s_chunks[k](pc, regs);" % prefix)
        o.append("            continue;")
        o.append("        }")
        o.append("#ifndef RC_MIPS")
        o.append("        {   /* a procedure of the segment by the address of its C wrapper */")
        o.append("            u32 ps_ = %s_native_lookup(pc);" % prefix)
        o.append("            if (ps_) {")
        o.append("                pc = ps_;")
        o.append("                continue;")
        o.append("            }")
        o.append("        }")
        o.append("#endif")
        o.append("        regs->ld_reg = 0;")
        o.append("        if (pc == RC_RETURN) {")
        o.append("            return regs->r[2];")
        o.append("        }")
        o.append("        {   /* a C function (a callback or a procedure outside the segment) */")
        o.append("            u32 r28 = regs->r[28];")
        o.append("            (void)r28;")
        o.append("            regs->r[2] = RC_EXT_CALL(pc, regs->r[4], regs->r[5], regs->r[6], regs->r[7], regs->r[29]);")
        o.append("        }")
        o.append("        pc = regs->r[31];")
        o.append("    }")
        o.append("}")
        open(out, "w").write("\n".join(o) + "\n")

    def emit_port(self, out, entries, prefix, defined, data, rename=None, at=()):
        """The C side of the port: a wrapper with the C calling convention for
        each entry the C code uses (unless a C version is defined), the table
        from their addresses back to the segment, the data words of the
        segment."""
        o = ["/* Generated by tools/recomp.py: the entries of the recompiled segment for the C code. */",
             '#include "nd_recomp.h"', ""]
        o.append("u32 %s_exec(u32 pc, RcRegs *regs);\n" % prefix)
        wrapped = []
        for name, addr in entries:
            if name not in self.visible or name in defined:
                continue
            if name in data:
                if name not in at:  # (at: the link puts it at its PlayStation address)
                    o.append("u32 %s; /* a data word of the segment */\n" % name)
                continue
            name = (rename or {}).get(name, name)
            wrapped.append((name, addr))
            o.append("u32 %s(u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5, u32 a6, u32 a7)\n{" % name)
            o.append("    return rc_call(%s_exec, 0x%08Xu, a0, a1, a2, a3, a4, a5, a6, a7);\n}\n" % (prefix, addr))
        o.append("static const u32 rc_native[][2] = {")
        for name, addr in wrapped:
            o.append("    { (u32)%s, 0x%08Xu }," % (name, addr))
        o.append("};\n")
        o.append("u32 %s_native_lookup(u32 addr)\n{" % prefix)
        o.append("    u32 i;\n")
        o.append("    for (i = 0; i < sizeof(rc_native) / sizeof(rc_native[0]); i++) {")
        o.append("        if (rc_native[i][0] == addr) {")
        o.append("            return rc_native[i][1];")
        o.append("        }")
        o.append("    }")
        o.append("    return 0;\n}")
        open(out, "w").write("\n".join(o) + "\n")


def c_names(paths):
    words = set()
    for p in paths:
        words |= set(re.findall(r"\b\w+\b", open(p).read()))
    return words


def c_defined(paths):
    """Names of the functions defined in C files."""
    out = set()
    for p in paths:
        out |= set(re.findall(r"^[A-Za-z][\w \*]*?\b(\w+)\([^;]*$", open(p).read(), re.M))
    return out


def main():
    ap = argparse.ArgumentParser(description="MIPS assembly of a segment -> C")
    ap.add_argument("segment")
    ap.add_argument("out")
    ap.add_argument("--prefix", default="nd")
    ap.add_argument("--visible", nargs="*", default=[], help="C files of the game: the symbols of the segment they use")
    ap.add_argument("--defined", nargs="*", default=[], help="C files with C versions of entries (no wrapper)")
    ap.add_argument("--data", nargs="*", default=[], help="entries that are data words")
    ap.add_argument("--at", nargs="*", default=[], help="data entries the link puts at their PlayStation address")
    ap.add_argument("--port", help="also write the wrappers and the table for the port here")
    ap.add_argument("--rename", nargs="*", default=[], help="old=new: names of the wrappers in the port")
    args = ap.parse_args()
    insns, entries = parse(args.segment)
    entries = [e for e in entries if isinstance(e, tuple)]
    lo, hi = insns[0].addr, insns[-1].addr + 4
    g = Gen(insns, lo, hi)
    words = c_names(args.visible)
    g.visible = set(n for n, _ in entries if n in words)
    g.emit(args.out, entries, args.prefix)
    if args.port:
        g.emit_port(args.port, entries, args.prefix, c_defined(args.defined), set(args.data),
                    dict(r.split("=") for r in args.rename), set(args.at))
    for a, t in g.hazards:
        print("load delay at %08X: %s" % (a, t), file=sys.stderr)


if __name__ == "__main__":
    main()
