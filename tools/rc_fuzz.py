"""Random-state test of the recompiled nd_asm (tools/recomp.py).

Starts the original and the recompiled code at a random label of the segment
with the same random registers, memory (full of pointers into itself and
into the scratchpad), scratchpad and GTE state, and compares what they did:
how they ended (return, the same fault at the same address, or running out
of steps), the memory and the scratchpad, the GTE state and, on a return,
the registers. The recompiled code makes the same memory accesses in the same
order, so even a run that ends in a fault is compared up to the fault. This
reaches the renderer procedures, which have no argument generators.

The original runs in unicorn, which has no load delay: the test performs it
at the loads in the delay slots of jr/jalr (as the recompiled code does).

usage: rc_fuzz.py EXE RC_ELF ASM [count] [seed]
"""
import os
import random
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gte_hook import GteHook  # noqa: E402
import recomp  # noqa: E402

from unicorn import Uc, UC_ARCH_MIPS, UC_MODE_MIPS32, UC_MODE_LITTLE_ENDIAN, UcError
from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_INVALID
from unicorn.mips_const import UC_MIPS_REG_0, UC_MIPS_REG_PC, UC_MIPS_REG_SP, UC_MIPS_REG_GP, UC_MIPS_REG_RA
from unicorn.mips_const import UC_MIPS_REG_HI, UC_MIPS_REG_LO

EXE, ELF, ASM = sys.argv[1], sys.argv[2], sys.argv[3]
COUNT = int(sys.argv[4]) if len(sys.argv) > 4 else 1000
SEED = int(sys.argv[5]) if len(sys.argv) > 5 else 1
STEPS = int(os.environ.get("STEPS", 20000))

RET = 0x80000100  # = RC_RETURN of the test build
BUF, BUF_SIZE = 0x80100000, 0x10000
SCR = 0x1F800000
STACK = 0x801F0000  # $sp of the original code (and of the recompiled code's registers)
C_SP = 0x801E0000  # the real stack of the recompiled C
RC_GTE_OP = 0x801FF000
RC_STEP = 0x801FF100  # rc_step of a build with RC_TRACE
TRAP = 0x80000200  # where a jump out of the segment ends


def main():
    data = open(EXE, "rb").read()
    t_addr, t_size = struct.unpack_from("<II", data, 0x18)
    text = data[0x800:0x800 + t_size]
    blob = ELF + ".bin"
    subprocess.run(["mipsel-linux-gnu-objcopy", "-O", "binary", "-j", ".text", "-j", ".rodata", "-j", ".data",
                    "-j", ".bss", ELF, blob], check=True)
    vm = open(blob, "rb").read()
    syms = {}
    for line in subprocess.run(["mipsel-linux-gnu-nm", ELF], capture_output=True, text=True).stdout.splitlines():
        p = line.split()
        if len(p) == 3:
            syms[p[2]] = int(p[0], 16)

    insns, entries = recomp.parse(ASM)
    lo, hi = insns[0].addr, insns[-1].addr + 4
    gen = recomp.Gen(insns, lo, hi)
    gen.emit(os.devnull, [e for e in entries if isinstance(e, tuple)], "nd")
    by_addr = gen.by_addr
    pending = dict(gen.pending_cases())  # target: registers that may see the load delay
    labels = set(a for _, a in gen.entries)
    for tok in open(ASM).read().split():
        if tok.startswith(".L") and tok.endswith(":"):
            labels.add(int(tok[2:-1], 16))
    labels = sorted(a for a in labels if a in by_addr)

    mu = Uc(UC_ARCH_MIPS, UC_MODE_MIPS32 + UC_MODE_LITTLE_ENDIAN)
    mu.mem_map(0, 0x400000)
    mu.mem_map(SCR, 0x1000)
    mu.mem_write(t_addr - 0x80000000, text)
    mu.mem_write(0x200000, vm)

    fault = []
    overflow = [False]

    def on_invalid(uc, access, addr, size, value, user):
        fault.append((access, addr & 0xFFFFFFFF))
        return False

    mu.hook_add(UC_HOOK_MEM_INVALID, on_invalid)

    # the load delay of the original at the loads in the delay slots of
    # jr/jalr: the instruction at the target reads the old value
    state = {"jump": None, "pend": None, "restore": None, "ext": None, "fix": None}

    def reg(n):
        return mu.reg_read(UC_MIPS_REG_0 + n)

    def setreg(n, v):
        if n:
            mu.reg_write(UC_MIPS_REG_0 + n, v & 0xFFFFFFFF)

    # A jump out of the segment (a callback, or a random address) ends the
    # run on both sides as "a call of X": the jump goes to TRAP instead,
    # after its delay slot (unicorn crashes on later runs after a fetch from
    # unmapped memory).
    def leave(rs, target):
        if (lo <= target < hi and target % 4 == 0) or target == RET:
            return
        state["ext"] = target
        state["fix"] = (rs, reg(rs))
        setreg(rs, TRAP)

    def on_jump(uc, addr, size, user):
        n = by_addr[addr]
        state["jump"] = reg(n.rs)
        leave(n.rs, state["jump"])

    def on_delay(uc, addr, size, user):
        f, state["fix"] = state["fix"], None
        if f:
            setreg(*f)

    def on_tramp(uc, addr, size, user):
        leave(8, reg(8))

    def on_trap(uc, addr, size, user):
        uc.emu_stop()

    def on_load(uc, addr, size, user):
        t = by_addr[addr].load_target()
        state["pend"] = (t, reg(t), state["jump"])

    def on_target(uc, addr, size, user):
        p, state["pend"] = state["pend"], None
        if p and p[2] == addr and p[0] in pending[addr]:
            t, old, _ = p
            state["restore"] = (t, reg(t), addr)
            setreg(t, old)

    def on_after(uc, addr, size, user):
        r = state["restore"]
        if r and r[2] + 4 == addr:
            state["restore"] = None
            t, new, a = r
            n = by_addr[a]
            if n.is_jump() or t not in n.writes():
                setreg(t, new)

    # the targets first: a jump there reads the old value too
    for a in pending:
        mu.hook_add(UC_HOOK_CODE, on_target, begin=a, end=a)
        mu.hook_add(UC_HOOK_CODE, on_after, begin=a + 4, end=a + 4)
        # a target that is a jump: its delay slot sees the new value; the
        # hook at a + 4 runs before the delay slot instruction
    for i, n in enumerate(insns[:-1]):
        d = insns[i + 1]
        if n.kind() in ("jr", "jalr"):
            mu.hook_add(UC_HOOK_CODE, on_jump, begin=n.addr, end=n.addr)
            mu.hook_add(UC_HOOK_CODE, on_delay, begin=d.addr, end=d.addr)
            if d.load_target():
                mu.hook_add(UC_HOOK_CODE, on_load, begin=d.addr, end=d.addr)
    # the jalr $t0 of the trampoline of the recompiled code (rc_ext_tramp)
    tr = syms["rc_ext_tramp"]
    tr_jalr = next(a for a in range(tr, tr + 64, 4)
                   if struct.unpack("<I", mu.mem_read(a - 0x80000000, 4))[0] == 0x0100F809)
    mu.hook_add(UC_HOOK_CODE, on_tramp, begin=tr_jalr, end=tr_jalr)
    mu.hook_add(UC_HOOK_CODE, on_delay, begin=tr_jalr + 4, end=tr_jalr + 4)
    mu.mem_write(TRAP - 0x80000000, b"\0" * 8)
    mu.hook_add(UC_HOOK_CODE, on_trap, begin=TRAP, end=TRAP)
    # add/addi/sub that overflow (an exception on the R3000; unicorn reports
    # it with pc 0, so the test checks the operands before)
    def on_add(uc, addr, size, user):
        n = by_addr[addr]
        x = reg(n.rs)
        y = (n.simm & 0xFFFFFFFF) if n.op == 8 else reg(n.rt)
        if n.op == 0 and n.fn == 0x22:
            y = (-y) & 0xFFFFFFFF
            if reg(n.rt) == 0x80000000:
                overflow[0] = not (x & 0x80000000)
                return
        z = (x + y) & 0xFFFFFFFF
        if not ((x ^ y) & 0x80000000) and ((x ^ z) & 0x80000000):
            overflow[0] = True

    for n in insns:
        if n.op == 8 or (n.op == 0 and n.fn in (0x20, 0x22)):
            mu.hook_add(UC_HOOK_CODE, on_add, begin=n.addr, end=n.addr)


    # CASE=n: only case n, with the instructions both sides ran (needs a
    # build with RC_TRACE=1 for the recompiled side)
    only = int(os.environ["CASE"]) if os.environ.get("CASE") else None
    steps = []
    mu.mem_write(RC_STEP - 0x80000000, struct.pack("<2I", 0x03E00008, 0))
    if only is not None:
        mu.hook_add(UC_HOOK_CODE, lambda uc, a, sz, u: steps.append(reg(4)), begin=RC_STEP, end=RC_STEP)
        mu.hook_add(UC_HOOK_CODE, lambda uc, a, sz, u: steps.append(a), begin=lo, end=hi - 4)

    # div/divu as the R3000 does them (unicorn differs on division by zero):
    # replaced by nop and done in a hook, which works in delay slots too
    def on_div(uc, addr, size, user):
        n = by_addr[addr]
        a, b = reg(n.rs), reg(n.rt)
        if n.fn == 0x1A:
            if b == 0:
                q, rem = (1 if a & 0x80000000 else 0xFFFFFFFF), a
            elif a == 0x80000000 and b == 0xFFFFFFFF:
                q, rem = a, 0
            else:
                sa = a - (1 << 32) if a & 0x80000000 else a
                sb = b - (1 << 32) if b & 0x80000000 else b
                q = abs(sa) // abs(sb) * (1 if (sa < 0) == (sb < 0) else -1)
                rem = sa - q * sb
        else:
            q, rem = (0xFFFFFFFF, a) if b == 0 else (a // b, a % b)
        mu.reg_write(UC_MIPS_REG_LO, q & 0xFFFFFFFF)
        mu.reg_write(UC_MIPS_REG_HI, rem & 0xFFFFFFFF)

    for n in insns:
        if n.op == 0 and n.fn in (0x1A, 0x1B):
            mu.mem_write(n.addr - 0x80000000, b"\0\0\0\0")
            mu.hook_add(UC_HOOK_CODE, on_div, begin=n.addr, end=n.addr)

    # the GTE after the hooks above: a GTE instruction at a jump target must
    # see the load delay (unicorn runs the hooks in the order they were added)
    gte = GteHook(mu, [(lo, hi)])
    gte.hook_call(RC_GTE_OP)
    gte.on_fault = lambda access, addr: fault.append((access, addr))

    # data accesses of the recompiled code to the segment itself: in the port
    # there is nothing at the PlayStation addresses of the segment
    seg_access = {}

    def on_seg(uc, access, addr, size, value, user):
        pc = mu.reg_read(UC_MIPS_REG_PC)
        if pc >= 0x80200000:
            seg_access[addr] = seg_access.get(addr, 0) + 1

    from unicorn import UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
    mu.hook_add(UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, on_seg, begin=lo, end=hi - 1)

    # coverage: the instructions of the segment the original ran
    covered = set()

    def on_block(uc, addr, size, user):
        covered.update(range(addr, addr + size, 4))

    from unicorn import UC_HOOK_BLOCK
    mu.hook_add(UC_HOOK_BLOCK, on_block, begin=lo, end=hi - 4)

    # every run starts from the same memory (a run can write anywhere)
    snapshot = bytes(mu.mem_read(0, 0x400000))
    # and the same CPU state (a fault in a delay slot leaves unicorn in it)
    ctx = mu.context_save()
    scr0 = bytes(mu.mem_read(SCR, 0x1000))

    r = random.Random(SEED)

    def ptr(r):
        k = r.randrange(10)
        if k < 6:
            return BUF + (r.randrange(0, BUF_SIZE - 0x400) & ~3)
        if k < 9:
            return SCR + (r.randrange(0, 0x300) & ~3)
        return BUF + r.randrange(0, BUF_SIZE)

    def value(r):
        k = r.randrange(10)
        if k < 4:
            return ptr(r)
        if k < 7:
            return r.randrange(-0x400, 0x400) & 0xFFFFFFFF
        return r.getrandbits(32)

    def words(r, n):
        return struct.pack("<%dI" % n, *[value(r) for _ in range(n)])

    def run(side, start, regs, steps):
        del fault[:]
        overflow[0] = False
        state.update(jump=None, pend=None, restore=None, ext=None, fix=None)
        try:
            if side == "asm":
                for n in range(1, 32):
                    setreg(n, regs[n])
                mu.reg_write(UC_MIPS_REG_HI, regs[32])
                mu.reg_write(UC_MIPS_REG_LO, regs[33])
                mu.emu_start(start, RET, count=steps)
            else:
                mu.mem_write(syms["rc_target"] - 0x80000000, struct.pack("<I", start))
                mu.mem_write(syms["rc_state"] - 0x80000000, struct.pack("<36I", 0, *regs[1:32], regs[32], regs[33], 0, 0))
                mu.reg_write(UC_MIPS_REG_SP, C_SP)
                mu.reg_write(UC_MIPS_REG_GP, 0x80060878)
                mu.reg_write(UC_MIPS_REG_RA, RET)
                mu.emu_start(syms["rc_go_state"], RET, count=steps)
        except UcError as e:
            return ("fault", str(e).split(" (")[0], tuple(fault[:1]))
        if state["ext"] is not None and mu.reg_read(UC_MIPS_REG_PC) == TRAP:
            return ("call", state["ext"])
        if mu.reg_read(UC_MIPS_REG_PC) != RET:
            return ("steps",)
        if side == "asm":
            out = [reg(n) for n in range(1, 32)] + [mu.reg_read(UC_MIPS_REG_HI), mu.reg_read(UC_MIPS_REG_LO)]
        else:
            out = list(struct.unpack("<34I", mu.mem_read(syms["rc_state"] - 0x80000000, 34 * 4)))[1:]
        return ("return", tuple(out))

    stats = {"overflow": 0, "call": 0, "return": 0, "fault": 0, "steps": 0, "differ": 0, "c_steps": 0}
    shown = 0
    # the loads in the delay slots of jr/jalr: a case for each jump and each
    # target whose instruction reads the loaded register (random cases
    # hardly ever get there)
    delay_cases = []
    for i, n in enumerate(insns[:-1]):
        d = insns[i + 1]
        t = d.load_target()
        if n.kind() in ("jr", "jalr") and t:
            for a, regs_ in sorted(pending.items()):
                if t in regs_ and n.rs != t:
                    delay_cases.append((n.addr, n.rs, a))
    total = COUNT + 3 * len(delay_cases)
    stats["delay"] = 0

    for case in range(total):
        regs = [0] + [value(r) for _ in range(31)] + [r.getrandbits(32), r.getrandbits(32)]
        if case < COUNT:
            start = r.choice(labels)
        else:
            start, rs, target = delay_cases[(case - COUNT) // 3]
            regs[rs] = target
        regs[29] = r.choice([STACK, ptr(r)])
        # the renderer's conventions: $v1 the scratchpad frame, $fp, $gp,
        # $s7 pointers (DrawContext, the object, the next primitive)
        if r.randrange(2):
            regs[3] = SCR
        for n in (28, 30, 23):
            if r.randrange(3):
                regs[n] = BUF + (r.randrange(0, BUF_SIZE - 0x2000) & ~3)
        regs[31] = r.choice([RET, RET, value(r)])
        mem = words(r, BUF_SIZE // 4)
        scr = words(r, 0x100)
        stk = words(r, 0x100)
        gd = [r.getrandbits(32) for _ in range(32)]
        gc = [r.getrandbits(32) for _ in range(32)]
        if only is not None and case != only:
            continue
        res = []
        traces = []
        for side in ("asm", "C"):
            del steps[:]
            mu.mem_write(0, snapshot)
            mu.context_restore(ctx)
            mu.mem_write(SCR, scr0)
            mu.mem_write(BUF - 0x80000000, mem)
            mu.mem_write(SCR, scr)
            mu.mem_write(STACK - 0x200 - 0x80000000, stk)
            gte.reset(gd, gc)
            end = run(side, start, regs, STEPS if side == "asm" else STEPS * 40)
            if side == "asm":
                asm_overflow = overflow[0]
            ram = bytes(mu.mem_read(0, 0x200000))
            # the game's memory, without the real stack of the recompiled C
            ram = ram[:C_SP - 0x8000 - 0x80000000] + ram[C_SP - 0x80000000:]
            res.append((end, ram, bytes(mu.mem_read(SCR, 0x400)),
                        bytes(mu.mem_read(STACK - 0x200 - 0x80000000, 0x400)),
                        tuple(gte.state.data), tuple(gte.state.ctrl)))
            # the C side runs the segment only through rc_step
            traces.append([x for x in steps if side == "C" or True])
        if only is not None:
            ta = traces[0]
            tc = [x for x in traces[1] if lo <= x < hi]
            print("asm:", " ".join("%x" % x for x in ta[-60:]))
            print("C:  ", " ".join("%x" % x for x in tc[-60:]))
        a, c = res
        if a[0][:2] == ("fault", "Unhandled CPU exception") and asm_overflow:
            # add/addi/sub overflowed: an exception on the R3000, which the
            # recompiled code does not raise (the game never overflows)
            stats["overflow"] += 1
            continue
        if a[0][0] == "steps":
            stats["steps"] += 1
            continue
        if c[0][0] == "steps":
            stats["c_steps"] += 1
        if a == c:
            stats[a[0][0]] += 1
            stats["delay"] += case >= COUNT
            continue
        stats["differ"] += 1
        if shown < 8:
            shown += 1
            what = [n for n, i in (("end", 0), ("RAM", 1), ("scratchpad", 2), ("stack", 3), ("GTE data", 4),
                                   ("GTE ctrl", 5)) if a[i] != c[i]]
            print("case %d, start %08X: differ (%s)" % (case, start, ", ".join(what)))
            if a[0] != c[0]:
                print("  asm:", str(a[0])[:300])
                print("  C:  ", str(c[0])[:300])
            elif a[0][0] == "return":
                print("  registers:", [(recomp.REG[n + 1], hex(x), hex(y)) for n, (x, y) in enumerate(zip(a[0][1], c[0][1]))
                                       if x != y])
            for i in (1, 2, 3):
                if a[i] != c[i]:
                    k = next(j for j in range(len(a[i])) if a[i][j] != c[i][j])
                    print("  %s first differs at +0x%X" % (("RAM", "scratchpad", "stack")[i - 1], k))
    print("%d cases (%d from %d labels): %d returned, %d called out, %d faulted (the same fault), %d skipped (the original ran out "
          "of steps), %d skipped (an overflow exception), %d differ" % (total, COUNT, len(labels), stats["return"], stats["call"], stats["fault"], stats["steps"], stats["overflow"], stats["differ"]))
    print("coverage: %d of %d instructions of the segment ran (%.0f%%)" %
          (len(covered & set(by_addr)), len(insns), 100.0 * len(covered & set(by_addr)) / len(insns)))
    print("of them %d cases at %d jumps to a target that reads the register loaded in the delay slot: %d match"
          % (3 * len(delay_cases), len(delay_cases), stats["delay"]))
    if seg_access:
        print("the recompiled code accessed the segment as data at %d addresses: %s" %
              (len(seg_access), " ".join("%08X" % a for a in sorted(seg_access)[:20])))
    if os.environ.get("UNCOVERED"):
        with open(os.environ["UNCOVERED"], "w") as f:
            run_start = None
            for n in insns + [None]:
                if n is not None and n.addr not in covered:
                    run_start = run_start or n.addr
                elif run_start:
                    f.write("%08X-%08X %d\n" % (run_start, n.addr if n else hi, ((n.addr if n else hi) - run_start) // 4))
                    run_start = None
    sys.exit(1 if stats["differ"] else 0)


main()
