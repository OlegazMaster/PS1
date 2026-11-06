"""Function-level differential test of src/nd.c against nd_asm.

Calls each original function (in the built executable) and its C version
(nd.elf, linked at 0x80180000) with the same random arguments in unicorn and
compares the return values and the memory they write.

usage: nd_difftest.py EXE ND_ELF [count] [seed]
"""
import random
import struct
import subprocess
import sys

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


# name: number of integer arguments
FUNCS = {
    "func_8003C2F0": 1,
    "func_8003C364": 1,
    "func_8003AE90": 2,
    "func_8003AEEC": 3,
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
    mu.mem_write(t_addr - 0x80000000, text)
    mu.mem_write(0x180000, vm)

    def call(addr, args):
        for reg, v in zip((UC_MIPS_REG_A0, UC_MIPS_REG_A1, UC_MIPS_REG_A2, UC_MIPS_REG_A3), args):
            mu.reg_write(reg, v & 0xFFFFFFFF)
        mu.reg_write(UC_MIPS_REG_SP, 0x801F0000)
        mu.reg_write(UC_MIPS_REG_GP, 0x80060878)
        mu.reg_write(UC_MIPS_REG_RA, RET)
        try:
            mu.emu_start(addr, RET, count=1000000)
        except UcError as e:
            return "error " + str(e)
        return mu.reg_read(UC_MIPS_REG_V0)

    r = random.Random(SEED)
    bad = 0
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
