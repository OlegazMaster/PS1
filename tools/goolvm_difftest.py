"""Differential test of src/goolvm.c against the original GOOL interpreter.

Runs random GOOL programs through the original code (gool_asm in the built
executable) and through the C version (goolvm.elf, linked at 0x80180000) in
unicorn, with the C instructions they call replaced by logging stubs, and
compares the results, the memory and the call logs. The GTE instructions
(opcode 0x51 draws a line through it) run on the software GTE of the port
(tools/gte_hook.py). Not covered: 0x49 (native code; the C version does not
support it). Programs where both versions crash or loop are skipped.

usage: goolvm_difftest.py EXE GOOLVM_ELF [count] [seed]
"""
import os
import random
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gte_hook import GteHook  # noqa: E402

from unicorn import Uc, UC_ARCH_MIPS, UC_MODE_MIPS32, UC_MODE_LITTLE_ENDIAN, UC_HOOK_CODE, UcError
from unicorn.mips_const import *

EXE, VM_ELF = sys.argv[1], sys.argv[2]
COUNT = int(sys.argv[3]) if len(sys.argv) > 3 else 500
SEED = int(sys.argv[4]) if len(sys.argv) > 4 else 1

RAM = 0x200000
SCR = 0x1F800000
RET_MAGIC = 0x80000100

OBJ = 0x80150000
LINKS = [0x80152000, 0x80154000, 0x80156000]
STACK = 0x8015A000  # sp starts at STACK + 0x800
ENT_G = 0x80158000  # Entry of obj->global / unk10
ENT_S = 0x80158100  # Entry of obj->external
GDATA = 0x80168000
SDATA = 0x80169000
ARRAY = 0x8016C000  # 4K words; pointers point into its middle
TABLE = 0x80170000  # SCRATCH(0x58) table, 4K words
CODE = 0x80178000
OUT = 0x8017F000
DC = 0x80140000  # a draw buffer for context.cur (opcode 0x51)
PRIMS = 0x80148000
CONTEXT_CUR = 0x80061A78 + 0xC
SUB = 200  # subroutine at CODE + SUB words

ASM_ENTRY = 0x80038E28
ASM_DIVS = [0x80038F70, 0x80038F98, 0x800391B4, 0x800393B8, 0x80039418]
DISPATCH = 0x8005D120

STUBS = {
    0x80035ABC: "C", 0x80035AC4: "C", 0x80035BE0: "C", 0x80035D08: "C",
    0x80036B88: "C", 0x80037300: "C", 0x800376F8: "C", 0x8003780C: "C",
    0x800378F8: "C", 0x80037A00: "C", 0x80037AE4: "C", 0x800385C0: "C",
    0x8003864C: "C", 0x800386BC: "C", 0x800387A8: "C", 0x800388C8: "C",
    0x800389C8: "C",
    0x80037BEC: "F", 0x80037CD8: "F", 0x80037DAC: "F", 0x80037EDC: "F",
    0x8003808C: "F", 0x80038248: "F", 0x800383DC: "S",
    0x800356B8: "pad", 0x80020184: "vec", 0x8004F794: "atan",
}


def w32(v):
    return struct.pack("<I", v & 0xFFFFFFFF)


def load_exe():
    data = open(EXE, "rb").read()
    t_addr, t_size = struct.unpack_from("<II", data, 0x18)
    return t_addr, data[0x800:0x800 + t_size]


def load_vm():
    out = subprocess.run(["mipsel-linux-gnu-nm", VM_ELF], capture_output=True, text=True).stdout
    entry = int([l for l in out.splitlines() if l.endswith(" func_80038E28")][0].split()[0], 16)
    blob = VM_ELF + ".bin"
    subprocess.run(["mipsel-linux-gnu-objcopy", "-O", "binary", "-j", ".text", "-j", ".rodata", VM_ELF, blob], check=True)
    return entry, open(blob, "rb").read()


T_ADDR, EXE_TEXT = load_exe()
VM_ENTRY, VM_BLOB = load_vm()

SAFE_REGS = list(range(8, 0x1F)) + list(range(0x22, 0x40))


class Gen:
    def __init__(self, rng):
        self.r = rng

    def value(self):
        r = self.r
        k = r.randrange(9)
        if k == 0:
            return r.randrange(0x800)  # data
        if k == 1:
            return 0x800 | r.randrange(0x200)  # const << 8
        if k == 2:
            return 0xA00 | r.randrange(0x100)  # const << 4
        if k == 3:
            return 0xB00 | r.randrange(0x80)  # frame word
        if k == 4:
            return 0xB80 | r.randrange(0x80)  # 0 / 1
        if k == 5:
            return 0xC00 | (r.randrange(4) << 6) | r.randrange(0x40)  # linked object
        if k == 6:
            return 0xE1F  # pop
        return 0xE00 | r.choice(SAFE_REGS)

    def dst(self):
        r = self.r
        k = r.randrange(5)
        if k == 0:
            return r.randrange(0x80)  # frame
        if k == 1:
            return 0x400 | (r.randrange(4) << 6) | r.randrange(0x40)
        if k == 2:
            return 0x61F  # push
        return 0x600 | r.choice(SAFE_REGS)

    def reg6(self):
        return self.r.choice(SAFE_REGS + [0x1F, 0x1F])

    def ins(self, pos, length):
        r = self.r
        ops = list(range(0x00, 0x31)) + list(range(0x32, 0x49)) + [0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x51]
        ops.remove(0x1F)
        ops.remove(0x20)
        ops += [0x1F, 0x20]
        op = r.choice(ops)
        L, R = self.value(), self.value()
        if op in (0x1F, 0x20):
            R = 0x800 | r.randrange(0x200)
        if op in (0x2A, 0x4E):
            R = r.randrange(4)  # gdata[0..3] hold pointers into ARRAY
            L = 0x800 | r.randrange(0x200)
        if op == 0x47:
            R = 4 + r.randrange(2)  # gdata[4]: pointer to SUB, gdata[5]: 0
            L = 0x800 | r.randrange(4)
        word = (op << 24) | (L << 12) | R
        if op in (0x11, 0x12, 0x14, 0x17, 0x19, 0x27, 0x2B, 0x2C, 0x2E):
            word = (op << 24) | (L << 12) | self.dst()
        if op == 0x18:
            word = (op << 24) | (self.reg6() << 18) | (r.randrange(2) << 14) | r.choice([0x3FFF, SUB, r.randrange(0x100)])
        if op in (0x23, 0x24):
            word = (op << 24) | (r.randrange(0x40) << 15) | (r.randrange(1, 4) << 12) | R
        if op in (0x32, 0x33, 0x34):
            fwd = r.randrange(1, max(2, length - pos))
            word = (op << 24) | (self.reg6() << 14) | (r.randrange(3) << 10) | fwd
        if op in (0x35, 0x36, 0x37, 0x38, 0x39, 0x3C, 0x3D, 0x3E, 0x44, 0x45):
            word = (op << 24) | (self.reg6() << 14) | (r.randrange(3) << 10) | r.randrange(0x400)
        if op == 0x3B:
            word = (op << 24) | (r.randrange(3) << 20) | (r.randrange(2) << 14) | SUB
        if op == 0x1A:
            word = (op << 24) | r.randrange(0x200000)
        return word


def setup(mu, rng, prog, sub, flags):
    """Fills memory for one test (identically for both runs)."""
    mu.mem_write(0, b"\0" * RAM)
    mu.mem_write(T_ADDR - 0x80000000, EXE_TEXT)
    mu.mem_write(0x180000, VM_BLOB)
    mu.mem_write(SCR, b"\0" * 0x1000)
    mu.mem_write(SCR + 0x5C, w32(DISPATCH))
    mu.mem_write(SCR + 0x54, w32(rng.randrange(1, 0x67)))
    mu.mem_write(SCR + 0x50, w32(rng.getrandbits(16)))
    mu.mem_write(SCR + 0x58, w32(TABLE + 0x2000))

    def rnd(addr, n):
        mu.mem_write(addr - 0x80000000, bytes(rng.getrandbits(8) for _ in range(n * 4)))

    def put(addr, v):
        mu.mem_write(addr - 0x80000000, w32(v))

    for o in [OBJ] + LINKS:
        rnd(o, 0x240)
    links = [0] + LINKS
    for o in [OBJ] + LINKS:
        for i in range(8):
            put(o + 0x40 + i * 4, rng.choice(links))
    for i in range(4):
        put(OBJ + 0x40 + i * 4, LINKS[i % 3] if i else OBJ)
    rnd(GDATA, 0x400)
    rnd(SDATA, 0x400)
    rnd(ARRAY, 0x1000)
    rnd(TABLE, 0x1000)
    rnd(STACK, 0x1000)
    for i in range(4):
        put(GDATA + i * 4, ARRAY + 0x2000 + rng.randrange(-64, 64) * 4)
    put(GDATA + 16, CODE + SUB * 4)
    put(GDATA + 20, 0)
    for e, d in ((ENT_G, GDATA), (ENT_S, SDATA)):
        put(e + 0x14, CODE)  # items[1]: code
        put(e + 0x18, d)  # items[2]: data
        put(e + 0x24, ARRAY + 0x2000)  # items[5]
    put(OBJ + 0x0C, ENT_G)
    put(OBJ + 0x10, ENT_G)
    put(OBJ + 0x14, ENT_S)
    sp = STACK + 0x800
    fp = sp - 0x40
    put(OBJ + 0xBC, sp)
    put(OBJ + 0xC4, fp)
    put(OBJ + 0xC0, CODE)
    # the frame at fp: flags, return pc, offsets (outermost when hi = 0)
    put(fp, rng.getrandbits(32))
    put(fp + 4, CODE + (SUB + 20) * 4)
    if rng.randrange(2):
        put(fp + 8, ((sp - 0x80 - OBJ - 0x40) << 16) | (sp - 0x20 - OBJ - 0x40))
        put(sp - 0x80 + 8, 0)
    else:
        put(fp + 8, rng.getrandbits(16))
    for i, wd in enumerate(prog):
        put(CODE + i * 4, wd)
    for i, wd in enumerate(sub):
        put(CODE + (SUB + i) * 4, wd)
    put(CODE + (SUB + 20) * 4, 0x31000000)
    put(OUT, 0)
    # a draw buffer for the line of opcode 0x51
    put(CONTEXT_CUR, DC)
    put(DC + 8, PRIMS)
    mu.mem_write(DC + 0x20 - 0x80000000, b"\xff\xff\xff\x00" * 0x800)


def run(entry, prog, sub, flags, seed):
    mu = Uc(UC_ARCH_MIPS, UC_MODE_MIPS32 + UC_MODE_LITTLE_ENDIAN)
    mu.mem_map(0, RAM)
    mu.mem_map(SCR, 0x1000)
    rng = random.Random(seed)
    setup(mu, rng, prog, sub, flags)
    log = []
    rets = random.Random(seed * 7 + 1)

    def rd(a):
        return struct.unpack("<I", mu.mem_read(a - 0x80000000 if a >= 0x80000000 else a, 4))[0]

    def hook(uc, addr, size, user):
        kind = STUBS.get(addr)
        if kind is None:
            return
        a = [uc.reg_read(r) for r in (UC_MIPS_REG_A0, UC_MIPS_REG_A1, UC_MIPS_REG_A2, UC_MIPS_REG_A3)]
        entry = [hex(addr), kind]
        if kind in ("C", "F", "S"):
            entry += [hex(a[0]), hex(a[1]), hex(rd(OBJ + 0xBC)), hex(rd(OBJ + 0xC4))]
            if kind != "C":
                entry.append(hex(a[2]))
        else:
            entry += [hex(a[0]), hex(a[1])]
        v = 0
        if kind in ("F", "S"):
            v = rets.choice([0, 0, 0, -0xFF & 0xFFFFFFFF, 0xF2])
        if kind == "S":
            mu.mem_write(SCR + 0x48, w32(rets.getrandbits(32)))
        if kind == "pad":
            v = (a[0] * 2654435761) & 0xFFFF
        if kind == "atan":
            v = (a[0] * 3 + a[1]) & 0xFFF
        if addr == 0x80035ABC:
            entry = entry[:2]  # takes no arguments
        log.append(entry)
        uc.reg_write(UC_MIPS_REG_V0, v)
        uc.reg_write(UC_MIPS_REG_PC, uc.reg_read(UC_MIPS_REG_RA))

    for addr in STUBS:
        mu.hook_add(UC_HOOK_CODE, hook, begin=addr, end=addr)

    gte = GteHook(mu, [(0x80038B40, 0x80039E3C), (0x80180000, 0x80180000 + len(VM_BLOB))])
    g = random.Random(seed * 3 + 2)
    ctrl = [0] * 32
    ctrl[24] = g.randrange(0, 320) << 16  # OFX
    ctrl[25] = g.randrange(0, 240) << 16  # OFY
    ctrl[26] = g.randrange(100, 1000)  # H
    ctrl[27] = g.randrange(-0x200, 0) & 0xFFFFFFFF  # DQA
    ctrl[28] = g.randrange(0, 0x1400000)  # DQB
    gte.reset(None, ctrl)

    def div_hook(uc, addr, size, user):
        # unicorn divides by zero unlike the R3000: do it as the R3000 does
        insn = struct.unpack("<I", uc.mem_read(addr - 0x80000000, 4))[0]
        rs, rt = (insn >> 21) & 31, (insn >> 16) & 31
        b = uc.reg_read(UC_MIPS_REG_0 + rt)
        if b != 0:
            return
        a = uc.reg_read(UC_MIPS_REG_0 + rs)
        if insn & 1:  # divu
            lo = 0xFFFFFFFF
        else:
            lo = 1 if a & 0x80000000 else 0xFFFFFFFF
        uc.reg_write(UC_MIPS_REG_LO, lo)
        uc.reg_write(UC_MIPS_REG_HI, a)
        uc.reg_write(UC_MIPS_REG_PC, addr + 4)

    for addr in ASM_DIVS:
        mu.hook_add(UC_HOOK_CODE, div_hook, begin=addr, end=addr)
    mu.reg_write(UC_MIPS_REG_SP, 0x801F0000)
    mu.reg_write(UC_MIPS_REG_GP, 0x80060878)
    mu.reg_write(UC_MIPS_REG_RA, RET_MAGIC)
    mu.reg_write(UC_MIPS_REG_A0, OBJ)
    mu.reg_write(UC_MIPS_REG_A1, flags)
    mu.reg_write(UC_MIPS_REG_A2, OUT)
    err = None
    try:
        mu.emu_start(entry, RET_MAGIC, count=200000)
    except UcError as e:
        err = "%s at %x" % (e, mu.reg_read(UC_MIPS_REG_PC))
    pc = mu.reg_read(UC_MIPS_REG_PC)
    if err is None and pc != RET_MAGIC:
        err = "no return (pc %x)" % pc
    regions = {}
    for name, a, n in (("obj", OBJ, 0x900), ("links", LINKS[0], 0x6000), ("stack", STACK, 0x4000),
                       ("data", GDATA, 0x2000), ("array", ARRAY, 0x4000), ("table", TABLE, 0x4000),
                       ("out", OUT, 4), ("seeds", 0x80060A2C, 8), ("ot", DC, 0x2020),
                       ("prims", PRIMS, 0x100), ("context", CONTEXT_CUR, 4)):
        regions[name] = bytes(mu.mem_read(a - 0x80000000, n))
    regions["scr"] = bytes(mu.mem_read(SCR, 0x5C))
    return err, mu.reg_read(UC_MIPS_REG_V0), regions, log


def main():
    rng = random.Random(SEED)
    fails = 0
    skipped = 0
    for t in range(COUNT):
        g = Gen(rng)
        n = rng.randrange(1, 24)
        prog = [g.ins(i, n) for i in range(n)] + [0x31000000]
        sub = [g.ins(i, 8) for i in range(rng.randrange(0, 8))] + [0x31000000]
        flags = rng.choice([0, 1, 8, 0x28, 0x10, 0x11, 0x18]) | (rng.getrandbits(16) << 16 if rng.randrange(2) else 0)
        seed = rng.getrandbits(32)
        a = run(ASM_ENTRY, prog, sub, flags, seed)
        c = run(VM_ENTRY, prog, sub, flags, seed)
        bad = []
        if a[0] is not None and c[0] is not None:
            skipped += 1
            continue
        if a[0] != c[0]:
            bad.append("error asm=%s c=%s" % (a[0], c[0]))
        if a[0] is None and a[1] != c[1]:
            bad.append("v0 asm=%x c=%x" % (a[1], c[1]))
        for k in a[2]:
            if a[2][k] != c[2][k]:
                off = next(i for i in range(len(a[2][k])) if a[2][k][i] != c[2][k][i])
                bad.append("mem %s +0x%x" % (k, off))
        if a[3] != c[3]:
            bad.append("calls asm=%s c=%s" % (a[3][:4], c[3][:4]))
        if bad:
            fails += 1
            if fails <= 15:
                print("test %d flags=%x prog=%s sub=%s" % (t, flags, [hex(x) for x in prog], [hex(x) for x in sub]))
                for b in bad:
                    print("   ", b)
    print("%d/%d tests differ (%d skipped: both crashed or looped)" % (fails, COUNT, skipped))


if __name__ == '__main__':
    main()
