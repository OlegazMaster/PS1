"""Function-level differential test of src/nd.c against nd_asm.

Calls each original function (in the built executable) and its C version
(nd.elf, linked at 0x80180000) with the same random arguments in unicorn and
compares the return values and the memory they write.

usage: nd_difftest.py EXE ND_ELF [count] [seed]
"""
import os
import random
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gte_hook import GteHook  # noqa: E402

from unicorn import Uc, UC_ARCH_MIPS, UC_MODE_MIPS32, UC_MODE_LITTLE_ENDIAN, UcError
from unicorn.mips_const import *

EXE, ELF = sys.argv[1], sys.argv[2]
COUNT = int(sys.argv[3]) if len(sys.argv) > 3 else 2000
SEED = int(sys.argv[4]) if len(sys.argv) > 4 else 1
RAM = 0x200000
RET = 0x80000100


def special(r):
    return r.choice([0, 1, -1, 0x7FFFFFFF, -0x80000000, 0x400, 0xFFF, 0x1000, 0xC00, 0x800])


def rint(r):
    k = r.randrange(4)
    if k == 0:
        return special(r)
    if k == 1:
        return r.randrange(-0x2000, 0x2000)
    if k == 2:
        return r.randrange(-0x1000000, 0x1000000)
    return r.getrandbits(32) - (1 << 31)


BUF = 0x80100000  # random memory for the functions that take pointers
BUF_SIZE = 0x8000
TICKS = 0x80060000  # replaced by the address of ticks_elapsed below
CB_8002B1CC = 0x8002B1CC  # the C callback the game passes to func_8003BD18


def gen_copy(r):
    n = r.choice([0, 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, r.randrange(1, 600)])
    src = BUF + 2 * r.randrange(0, 0x1000)
    dst = BUF + 0x2000 + 2 * r.randrange(0, 0x1000)
    return [src, dst, n]


def gen_ot(r):
    return [BUF + 4 * r.randrange(0, 0x100), r.choice([0x40, 0x80, 0x800 // 4, 0x7FF, 0x1000 // 4])]


def gen_grid(r):
    return [r.randrange(-32, 32), r.randrange(-32, 32)]


def box_list(r):
    words = []
    for _ in range(r.randrange(1, 4)):
        words += [r.randrange(1, 0x300) << 16, r.randrange(1, 0x300) | (r.randrange(1, 0x300) << 16),
                  r.randrange(0, 8) << 16, r.randrange(0, 8) | (r.randrange(0, 8) << 16)]
        for _ in range(r.randrange(0, 8)):
            lo = (r.randrange(1, 0x2000) << 3) | r.randrange(8)
            if lo & 0xFFFF == 0xFFFF:
                lo = 8
            words += [(lo & 0xFFFF) | ((r.randrange(-0x1000, 0x1000) & 0xFFFF) << 16),
                      (r.randrange(-0x1000, 0x1000) & 0xFFFF) | ((r.randrange(-0x1000, 0x1000) & 0xFFFF) << 16)]
    words.append(0xFFFF)
    return b"".join(struct.pack("<I", w & 0xFFFFFFFF) for w in words)


def gen_surfaces(r):
    rec, org, box, obj = BUF + 0x4000, BUF + 0x7F00, BUF + 0x7F40, BUF + 0x7000
    origin = [r.randrange(-0x40000, 0x40000) for _ in range(3)]
    lo = [origin[i] + r.randrange(-0x20000, 0x20000) for i in range(3)]
    hi = [lo[i] + r.randrange(0, 0x30000) for i in range(3)]
    patches = [(rec, box_list(r)), (org, struct.pack("<3i", *origin)), (box, struct.pack("<6i", *(lo + hi)))]
    args = [obj, rec, org, box, r.randrange(-0x40000, 0x40000), BUF + 0x7400,
            r.choice([-999999999, r.getrandbits(31)]), CB_8002B1CC]
    return args, patches


def gen_ceiling(r):
    args, patches = gen_surfaces(r)
    ceil = BUF + 0x7F80
    patches.append((ceil, struct.pack("<i", r.choice([0x7FFFFFFF, r.randrange(-0x40000, 0x40000)]))))
    return [args[0], args[1], args[2], args[3], ceil, r.choice([-999999999, r.getrandbits(31)]), r.randrange(2)], patches


def gen_tree(r):
    """A random surface tree for func_8003AF78 (depth at most 6, as the
    original keeps its stack in 7 registers)."""
    item = BUF + 0x4000
    dims = [r.choice([0, 1, 2, 3, 4, 4]) for _ in range(3)]
    data = bytearray(0x24)
    size = [r.randrange(1, 0x400) for _ in range(3)]
    origin = [r.randrange(-0x400, 0x400) for _ in range(3)]

    def node(depth):
        axes = sum(depth < d for d in dims)
        if axes == 0 or depth >= 6 or r.randrange(6) == 0:
            return r.choice([0, (r.randrange(1, 0x3FFF) << 1) | 1])
        nkids = 1 << axes
        kids = [node(depth + 1) for _ in range(nkids)]
        off = len(data)
        if off + 2 * nkids > 0x1F00:
            return 1
        for k in kids:
            data.extend(struct.pack("<H", k))
        if len(data) & 1:
            data.append(0)
        return off

    root = node(0)
    struct.pack_into("<6i", data, 0, *(origin + size))
    struct.pack_into("<4H", data, 0x1C, root, *dims)
    def edge(i):
        # often exactly on a node boundary (and so on the tests' edge cases)
        if r.randrange(2):
            step = (size[i] << 8) >> r.randrange(0, 5)
            return (origin[i] << 8) + step * r.randrange(-1, 17) + r.choice([0, 0, -1, 1])
        return (origin[i] << 8) + r.randrange(-0x20000, 0x40000)
    lo = [edge(i) for i in range(3)]
    hi = [max(lo[i], edge(i)) for i in range(3)]
    box = BUF + 0x7F40
    out = BUF + 0x6000
    patches = [(item, bytes(data)), (box, struct.pack("<6i", *(lo + hi)))]
    return [item, box, out, r.choice([0, 0, 3, r.randrange(0, 0x200)])], patches


def gen_delta(r):
    """A random delta of a halfword list for func_80039E3C/func_8003A234."""
    count = r.randrange(0, 80)
    items = [r.getrandbits(16) for _ in range(count)]

    def stream(with_values):
        out = []
        idx = 0
        for _ in range(r.randrange(0, 5)):
            idx += r.randrange(0, 12)
            n = r.randrange(1, 5)
            out.append(((n - 1) << 12) | min(idx, 0xFFF))
            if with_values:
                out += [r.getrandbits(16) for _ in range(n)]
            idx += n
        if r.randrange(3) == 0:
            out.append(0xFFFF)
        out.append(r.getrandbits(16))  # never read as a command
        return out

    first = stream(r.randrange(2) == 0)
    second = stream(r.randrange(2) == 0)
    swaps = []
    for i in range(r.randrange(0, 8)):
        k = r.randrange(4 if i else 3)  # no relative swap first
        a = r.randrange(0, 30)
        if k == 0:
            swaps.append(0x8000 | (r.randrange(16) << 11) | a)
        elif k == 1:
            swaps.append(0x2000 | (r.randrange(4) << 11) | a)
        elif k == 2:
            swaps += [a, r.randrange(0, 20)]
        else:
            swaps.append(0x4000 | (r.randrange(0, 8) << 5) | r.randrange(0, 8))
    if r.randrange(2):
        swaps.append(0xFFFF)
    body = first + second + swaps
    h2 = 4 + len(first) - 2
    h3 = h2 + len(second)
    h0 = h3 + len(swaps)
    delta = struct.pack("<4H", h0, 0, h2, h3) + struct.pack("<%dH" % len(body), *body)
    d, src, dst = BUF + 0x4000, BUF + 0x5000, BUF + 0x6000
    lst = struct.pack("<2H", count, 0) + struct.pack("<%dH" % count, *items)
    return [d, src, dst], [(d, delta), (src, lst)]


def gen_turn(r):
    m = [r.getrandbits(32) for _ in range(5)]
    pos = [r.randrange(-0x1000000, 0x1000000) for _ in range(3)]
    n = r.randrange(0, 12)
    pts = [r.randrange(-0x800000, 0x800000) for _ in range(3 * n)]
    src, dst, mat, p = BUF + 0x4000, r.choice([BUF + 0x4000, BUF + 0x5000]), BUF + 0x7E00, BUF + 0x7E40
    patches = [(src, struct.pack("<%di" % len(pts), *pts)), (mat, struct.pack("<5I", *m)), (p, struct.pack("<3i", *pos))]
    return [dst, src, mat, p, n, r.randrange(0, 12)], patches


def height_field(r):
    """A RenderState-like block (at BUF + 0x7800) for the height fields: the
    weight, two 32x32 height tables, the cell types and the walls."""
    rs = BUF + 0x7800
    lo, hi, types, walls = 0x80110000, 0x80111000, 0x80120000, 0x80130000
    p = [(lo, bytes(r.getrandbits(8) for _ in range(0x1000))),
         (hi, bytes(r.getrandbits(8) for _ in range(0x1000))),
         (types, bytes(r.getrandbits(8) for _ in range(0x10000))),
         (walls, bytes(r.choice([0, r.getrandbits(8), r.getrandbits(8)]) for _ in range(0x10000))),
         (rs + 0xA0, struct.pack("<5I", r.randrange(0, 0x1000), lo, hi, types, walls))]
    return rs, p


def gen_height(r):
    rs, p = height_field(r)
    pos = BUF + 0x7F00
    p.append((pos, struct.pack("<3i", r.randrange(-0x400000, 0x4000000), 0, r.randrange(-0x400000, 0x4000000))))
    return [r.choice([0, BUF + 0x7F80, BUF + 0x7F80]), pos, rs], p


def gen_ground(r):
    rs, p = height_field(r)
    obj, delta = BUF + 0x7000, BUF + 0x7F00
    cell = lambda: r.randrange(0, 256) << 18
    near = lambda c: c + r.choice([r.randrange(0, 0x40000), r.randrange(-0x800, 0x800), 0x40000 + r.randrange(-0x800, 0x800)])
    x, z = near(cell()), near(cell())
    p.append((obj + 0x60, struct.pack("<3i", x, r.randrange(-0x100000, 0x100000), z)))
    p.append((delta, struct.pack("<3i", *[r.choice([r.randrange(-0x60000, 0x60000), r.randrange(-0x2000, 0x2000)])
                                          for _ in range(3)])))
    return [r.choice([0, BUF + 0x7F80, BUF + 0x7F80]), obj, rs, delta], p


def gen_model(r):
    xform = [r.randrange(-0x4000000, 0x4000000) for _ in range(3)] + [r.getrandbits(16) for _ in range(3)] + \
            [r.choice([0x1000, r.randrange(0, 0x4000)]) for _ in range(3)]
    cam = [xform[i] + r.choice([r.randrange(-0x400000, 0x400000), r.randrange(-0x4000000, 0x4000000)]) for i in range(3)]
    rot = [r.getrandbits(32) for _ in range(5)]
    xf, cm, rt = BUF + 0x7E00, BUF + 0x7E40, BUF + 0x7E80
    p = [(xf, struct.pack("<9i", *xform)), (cm, struct.pack("<3i", *cam)), (rt, struct.pack("<5I", *rot))]
    return [xf, cm, r.choice([0, 0, 1]), r.randrange(0, 8), rt, r.randrange(-0x100, 0x800),
            r.choice([0, r.randrange(0, 0x3000)]), r.getrandbits(1)], p


def gen_point(r):
    """A point query into a random surface tree (func_8003C990)."""
    args, patches = gen_tree(r)
    item = args[0]
    data = patches[0][1]
    origin = struct.unpack_from("<3i", data, 0)
    size = struct.unpack_from("<3i", data, 12)
    pos = BUF + 0x7F00
    p = [(origin[i] << 8) + r.randrange(-0x100, (size[i] << 8) + 0x100) for i in range(3)]
    return [item, pos, r.choice([0, 1, 8, 9, 0x10])], patches[:1] + [(pos, struct.pack("<3i", *p))]


def gen_edges(r):
    """A list of collision boxes for func_8003B874, written into the buffer
    through the returned (address, bytes) patches."""
    words = []
    for _ in range(r.randrange(1, 4)):
        words += [r.randrange(1, 0x300) << 16, r.randrange(1, 0x300) | (r.randrange(1, 0x300) << 16),
                  r.randrange(0, 8) << 16, r.randrange(0, 8) | (r.randrange(0, 8) << 16)]
        for _ in range(r.randrange(0, 8)):
            lo = (r.randrange(1, 0x2000) << 3) | r.randrange(8)
            if lo & 0xFFFF == 0xFFFF:
                lo = 8
            words += [(lo & 0xFFFF) | ((r.randrange(-0x1000, 0x1000) & 0xFFFF) << 16),
                      (r.randrange(-0x1000, 0x1000) & 0xFFFF) | ((r.randrange(-0x1000, 0x1000) & 0xFFFF) << 16)]
    words.append(0xFFFF)
    rec = BUF + 0x4000
    org = BUF + 0x7F00
    origin = [r.randrange(-0x40000, 0x40000) for _ in range(3)]
    patches = [(rec, b"".join(struct.pack("<I", w & 0xFFFFFFFF) for w in words)),
               (org, b"".join(struct.pack("<i", v) for v in origin))]
    y = [r.randrange(-0x20000, 0x20000) for _ in range(3)]
    args = [rec, org, r.randrange(2), y[0], y[1], y[2],
            origin[0] + r.randrange(-0x10000, 0x10000), origin[2] + r.randrange(-0x10000, 0x10000)]
    return args, patches


# the original code of the functions that use the GTE
GTE_RANGES = {
    "func_8003F520": (0x8003F520, 0x8003F5D8),
    "matrix helpers": (0x80041EF0, 0x80042058),
    "height fields": (0x80044CA0, 0x800452A0),
    "func_8003C3D0": (0x8003C3D0, 0x8003C990),
}

# helpers that take a GTE matrix in t0..t4 in the original (a pointer to it
# in C): "m" (the matrix only) or "rt_m" (a0 = the matrix for RT first)
REGFUNCS = {
    "func_80041FBC": "m",
    "func_80041F18": "m",
    "func_80041EF0": "rt_m",
    "func_80041FD8": "m",
}

# functions on memory whose return value is compared too
RETURNS = {"func_8003C04C", "func_8003AF78", "func_800450D4", "func_80044CA0", "func_8003C3D0"}

# functions on memory whose result is (also) the state of the GTE
GTE_STATE = {"func_8003C3D0"}
RETURNS.add("func_8003C990")

# name: argument generator (functions on memory); the buffer and the tick
# counter are compared after the call
MEMFUNCS = {
    "func_8003A928": gen_copy,
    "func_8003AC44": gen_ot,
    "func_8003AE70": lambda r: [],
    "func_8003BBF0": gen_grid,
    "func_8003B874": gen_edges,
    "func_8003BD18": gen_surfaces,
    "func_8003C04C": gen_ceiling,
    "func_8003AF78": gen_tree,
    "func_80039E3C": gen_delta,
    "func_8003A234": gen_delta,
    "func_8003F520": gen_turn,
    "func_800450D4": gen_height,
    "func_80044CA0": gen_ground,
    "func_8003C3D0": gen_model,
    "func_8003C990": gen_point,
}

# name: number of integer arguments
FUNCS = {
    "func_8003C2F0": 1,
    "func_8003C364": 1,
    "func_8003AE90": 2,
    "func_8003AEEC": 3,
    "func_8003D9DC": 1,
}


def main():
    data = open(EXE, "rb").read()
    t_addr, t_size = struct.unpack_from("<II", data, 0x18)
    text = data[0x800:0x800 + t_size]
    blob = ELF + ".bin"
    subprocess.run(["mipsel-linux-gnu-objcopy", "-O", "binary", "-j", ".text", "-j", ".rodata", ELF, blob], check=True)
    vm = open(blob, "rb").read()
    syms = {}
    for line in subprocess.run(["mipsel-linux-gnu-nm", ELF], capture_output=True, text=True).stdout.splitlines():
        p = line.split()
        if len(p) == 3:
            syms[p[2]] = int(p[0], 16)
    mu = Uc(UC_ARCH_MIPS, UC_MODE_MIPS32 + UC_MODE_LITTLE_ENDIAN)
    mu.mem_map(0, RAM)
    mu.mem_map(0x1F800000, 0x1000)  # scratchpad
    mu.mem_write(t_addr - 0x80000000, text)
    mu.mem_write(0x180000, vm)
    # GTE instructions of the original segment and of the C version run on
    # the software GTE
    # (only in the functions tested: every hook slows down every instruction)
    gte = GteHook(mu, list(GTE_RANGES.values()) + [(0x80180000, 0x80180000 + len(vm))])

    def call(addr, args):
        for reg, v in zip((UC_MIPS_REG_A0, UC_MIPS_REG_A1, UC_MIPS_REG_A2, UC_MIPS_REG_A3), args):
            mu.reg_write(reg, v & 0xFFFFFFFF)
        mu.reg_write(UC_MIPS_REG_SP, 0x801F0000)
        for i, v in enumerate(args[4:]):  # arguments 5.. on the stack
            mu.mem_write(0x1F0000 + 0x10 + 4 * i, struct.pack("<I", v & 0xFFFFFFFF))
        mu.reg_write(UC_MIPS_REG_GP, 0x80060878)
        mu.reg_write(UC_MIPS_REG_RA, RET)
        try:
            mu.emu_start(addr, RET, count=1000000)
        except UcError as e:
            return "error %s at %x" % (e, mu.reg_read(UC_MIPS_REG_PC))
        return mu.reg_read(UC_MIPS_REG_V0)

    r = random.Random(SEED)
    bad = 0
    ticks = int(subprocess.run(["sh", "-c", "mipsel-linux-gnu-nm %s | grep ' ticks_elapsed$'" % EXE.replace("SCUS_942.44", "main.elf")],
                               capture_output=True, text=True).stdout.split()[0], 16)
    for name, gen in MEMFUNCS.items():
        orig = int(name[5:], 16)
        fails = 0
        skipped = 0
        for _ in range(COUNT // 10):
            args = gen(r)
            patches = []
            if isinstance(args, tuple):
                args, patches = args
            init = bytes(r.getrandbits(8) for _ in range(BUF_SIZE))
            t0 = r.getrandbits(32)
            scr = bytes(r.getrandbits(8) for _ in range(0x400))
            res = []
            for addr in (orig, syms[name]):
                mu.mem_write(BUF - 0x80000000, init)
                mu.mem_write(ticks - 0x80000000, struct.pack("<I", t0))
                mu.mem_write(0x1F800000, scr)
                gte.reset()
                for a, b in patches:
                    mu.mem_write(a - 0x80000000, b)
                v = call(addr, args)
                res.append((v if isinstance(v, str) or name in RETURNS else None,
                            bytes(mu.mem_read(BUF - 0x80000000, BUF_SIZE)) +
                            (bytes(gte.state.data) + bytes(gte.state.ctrl) if name in GTE_STATE else b""),
                            bytes(mu.mem_read(ticks - 0x80000000, 4)), bytes(mu.mem_read(0x1F800000, 0x400))))
            # the original saves its registers in the scratchpad (0x00-0x3F,
            # 0x70-0xA3 around calls, ra at 0x384) and its temporaries at
            # 0xE0-0xF3: not part of the result
            res = [(e, b, t, sc[0x40:0x70] + sc[0xA4:0xE0] + sc[0xF4:0x384] + sc[0x388:]) for e, b, t, sc in res]
            if isinstance(res[0][0], str) and isinstance(res[1][0], str):
                skipped += 1  # both crashed or looped on a malformed case
                continue
            if res[0] != res[1]:
                fails += 1
                if fails <= 5:
                    what = [n for n, i in (("error", 0), ("memory", 1), ("ticks", 2), ("scratchpad", 3)) if res[0][i] != res[1][i]]
                    print("%s%s: differ (%s) asm: %s / C: %s" % (name, tuple(hex(x) for x in args), ", ".join(what), res[0][0], res[1][0]))
        print("%s: %d/%d differ" % (name, fails, COUNT // 10) + (" (%d skipped: both crashed or looped)" % skipped if skipped else ""))
        bad += fails
    for name, kind in REGFUNCS.items():
        orig = int(name[5:], 16)
        fails = 0
        for _ in range(COUNT // 10):
            m = [r.getrandbits(32) for _ in range(5)]
            rt = [r.getrandbits(32) for _ in range(5)]
            data = [r.getrandbits(32) for _ in range(32)]
            ctrl = [r.getrandbits(32) for _ in range(32)]
            mem, rtp = BUF + 0x7D00, BUF + 0x7D40
            res = []
            for side in ("asm", "C"):
                mu.mem_write(mem - 0x80000000, struct.pack("<5I", *m))
                mu.mem_write(rtp - 0x80000000, struct.pack("<5I", *rt))
                gte.reset(data, ctrl)
                if side == "asm":
                    for i, v in enumerate(m):
                        mu.reg_write(UC_MIPS_REG_8 + i, v)
                    err = call(orig, [rtp])
                    out = [mu.reg_read(UC_MIPS_REG_8 + i) for i in range(5)]
                else:
                    err = call(syms[name], [rtp, mem] if kind == "rt_m" else [mem])
                    out = list(struct.unpack("<5I", mu.mem_read(mem - 0x80000000, 20)))
                res.append((err if isinstance(err, str) else None, out, list(gte.state.data), list(gte.state.ctrl)))
            if res[0] != res[1]:
                fails += 1
                if fails <= 3:
                    print("%s m=%s: differ: asm %s / C %s" % (name, [hex(x) for x in m], [hex(x) for x in res[0][1]], [hex(x) for x in res[1][1]]))
                    if res[0][2:] != res[1][2:]:
                        print("    GTE state differs")
        print("%s: %d/%d differ" % (name, fails, COUNT // 10))
        bad += fails
    for name, n in FUNCS.items():
        orig = int(name[5:], 16)
        fails = 0
        for _ in range(COUNT):
            args = [rint(r) for _ in range(n)]
            a, c = call(orig, args), call(syms[name], args)
            if a != c:
                fails += 1
                if fails <= 5:
                    print("%s%s: asm %s, C %s" % (name, tuple(hex(x) for x in args), a, c))
        print("%s: %d/%d differ" % (name, fails, COUNT))
        bad += fails
    sys.exit(1 if bad else 0)


main()
