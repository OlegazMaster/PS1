"""Builds the PC port (32-bit x86): build/pc/crash3.

The memory model of the port: the 2 MB of PlayStation RAM are at their own
address, 0x80000000, in the 32-bit process (src/pc/main.c maps them and
loads the user's SCUS_942.44 there). So the data of the game that is not C
(the data segment of the executable: tables, pointers between them) is used
in place, at its addresses, and the global variables of the C files are put
at the addresses they have in the PlayStation build (build/us/main.elf):
pointers in the data to them stay right. The scratchpad is mapped at
0x1F800000. The code is x86 at the usual place.

For that the C files are compiled with a section per variable, and a linker
script puts each section at its address inside the RAM (a NOLOAD section,
which the system maps as zeroed memory when it loads the program, like the
scratchpad section). Symbols of the data
segment become absolute symbols. A table of the C functions (PlayStation
address -> x86 address) lets the startup code replace pointers to functions
in the data.

usage: tools/pc/build.py [-j N]
"""
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "build", "pc")
MAIN_ELF = os.path.join(ROOT, "build", "us", "main.elf")
RC = os.path.join(ROOT, "build", "us", "rc")
CC = ["gcc", "-m32"]
# i686 without SSE: the variables are at their PlayStation addresses, which
# are not aligned as x86 compilers align arrays for vector instructions
CFLAGS = ["-march=i686", "-mno-sse", "-std=gnu99", "-O1", "-g", "-fno-pic", "-fno-common", "-fdata-sections", "-fno-strict-aliasing",
          "-fwrapv", "-funsigned-char", "-malign-double", "-DNON_MATCHING", "-DVERSION_US", "-DTARGET_PC",
          "-I" + os.path.join(ROOT, "include"), "-w"]
GAME = ["audio", "camera", "cdfs", "coll", "game", "gfx", "gool", "goolop", "goolvm", "interp", "level", "main",
        "math", "nd", "ns", "obj", "prop", "sys"]
RAM_LO, RAM_HI = 0x80000000, 0x80200000
# segments of hand-written assembly run as recompiled C: prefix, file, options
RECOMPILED = [
    ("nd", "nd_asm.s", ["--defined", os.path.join(ROOT, "src", "nd.c"), "--data", "ticks_elapsed"]),
    ("heap", "heap.s", ["--rename", "malloc=psx_malloc", "free=psx_free", "realloc=psx_realloc"]),
]
# the heap of the game (in the PlayStation RAM, so that the 24-bit addresses of
# the primitive lists work) under other names than those of the C library
HEAP_NAMES = ["-Dmalloc=psx_malloc", "-Dfree=psx_free", "-Drealloc=psx_realloc"]
# where the data of the executable begins (file offset 0x4C440 in
# config/us/splat.yaml): symbols there are data even when the assembly of
# the data segment typed them as text
DATA_LO = 0x8005BC40
EXE_END = 0x80061000  # the end of what the executable loads


def sdl_flags():
    """(cflags, libs) of a 32-bit SDL2, or None: without it the port runs
    without a window (frames can be written to files)."""
    env = dict(os.environ, PKG_CONFIG_PATH="/usr/lib/i386-linux-gnu/pkgconfig")
    r = subprocess.run(["pkg-config", "--cflags", "--libs", "sdl2"], capture_output=True, text=True, env=env)
    if r.returncode:
        return None
    c = subprocess.run(["pkg-config", "--cflags", "sdl2"], capture_output=True, text=True, env=env).stdout.split()
    l = subprocess.run(["pkg-config", "--libs", "sdl2"], capture_output=True, text=True, env=env).stdout.split()
    return c, l


SDL = sdl_flags()


def run(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode:
        sys.stderr.write(" ".join(cmd) + "\n" + r.stdout + r.stderr)
        sys.exit(1)
    return r.stdout


def psx_symbols():
    """name -> (address, type) of the PlayStation build."""
    syms = {}
    for line in run(["mipsel-linux-gnu-nm", MAIN_ELF]).splitlines():
        p = line.split()
        if len(p) == 3:
            syms.setdefault(p[2], (int(p[0], 16), p[1]))
    return syms


def compile_all():
    os.makedirs(OUT, exist_ok=True)
    jobs = [(os.path.join(ROOT, "src", g + ".c"), os.path.join(OUT, g + ".o")) for g in GAME]
    for f in sorted(os.listdir(os.path.join(ROOT, "src", "pc"))):
        if f.endswith(".c"):
            jobs.append((os.path.join(ROOT, "src", "pc", f), os.path.join(OUT, "pc_" + f[:-2] + ".o")))
    # the hand-written assembly the port runs as recompiled C (tools/recomp.py;
    # generated, not committed): the renderer and the heap of the game
    os.makedirs(RC, exist_ok=True)
    visible = [os.path.join(ROOT, "src", g + ".c") for g in GAME] + \
        [os.path.join(ROOT, "src", "pc", f) for f in os.listdir(os.path.join(ROOT, "src", "pc")) if f.endswith(".c")]
    for prefix, seg, extra in RECOMPILED:
        out, port = os.path.join(RC, prefix + "_recomp.c"), os.path.join(RC, prefix + "_port.c")
        run(["python3", os.path.join(ROOT, "tools", "recomp.py"), os.path.join(ROOT, "asm", "us", seg),
             out + ".new", "--prefix", prefix, "--visible"] + visible + extra + ["--port", port + ".new"])
        for f in (out, port):  # only a changed file is compiled again
            if os.path.exists(f) and open(f).read() == open(f + ".new").read():
                os.remove(f + ".new")
            else:
                os.replace(f + ".new", f)
            jobs.append((f, os.path.join(OUT, os.path.basename(f)[:-2] + ".o")))

    def one(job):
        src, obj = job
        if os.path.exists(obj) and os.path.getmtime(obj) > max(os.path.getmtime(src), newest_header()):
            return
        extra = ["-Dmain=game_main"] if src.endswith("/src/main.c") else []
        if src.endswith("/src/pc/video.c") and SDL:
            extra += ["-DPC_SDL"] + SDL[0]
        if "/src/pc/" not in src and "/rc/" not in src:
            extra += HEAP_NAMES
        run(CC + CFLAGS + extra + ["-c", src, "-o", obj])

    with ThreadPoolExecutor(os.cpu_count()) as ex:
        list(ex.map(one, jobs))
    return [o for _, o in jobs]


def newest_header():
    inc = os.path.join(ROOT, "include")
    return max(os.path.getmtime(os.path.join(inc, f)) for f in os.listdir(inc))


def sections(obj):
    """(section, size) of the data sections of an object file."""
    out = []
    for line in run(["objdump", "-h", obj]).splitlines():
        p = line.split()
        if len(p) >= 7 and p[0].isdigit():
            out.append((p[1], int(p[2], 16)))
    return out


def main():
    psx = psx_symbols()
    objs = compile_all()
    placed = []  # (address, size, object, section, name)
    unplaced = []
    game_objs = [os.path.join(OUT, g + ".o") for g in GAME]
    for o in game_objs:  # only the variables of the game (not of the port layer)
        for sec, size in sections(o):
            m = re.match(r"^\.(data|bss|rodata)\.(\w+)$", sec)
            if not m or size == 0:
                continue
            name = m.group(2)
            if name in psx and RAM_LO <= psx[name][0] < RAM_HI:
                placed.append((psx[name][0], size, o, sec, name))
            elif m.group(1) != "rodata":
                unplaced.append((o, sec, name))
    placed.sort()
    for (a, s, o, sec, n), (a2, _, o2, _, n2) in zip(placed, placed[1:]):
        if a + s > a2:
            print("warning: %s (%s, 0x%X bytes at %08X) overlaps %s at %08X" % (n, os.path.basename(o), s, a, n2, a2))

    # the linker script: the variables at their addresses in the RAM
    ld = ["SECTIONS", "{", "  .psxram 0x%08X (NOLOAD) : SUBALIGN(1)" % RAM_LO, "  {"]
    for a, s, o, sec, n in placed:
        ld.append("    . = 0x%08X - 0x%08X; %s(%s)" % (a, RAM_LO, o, sec))
    ld += ["    . = 0x%X;" % (RAM_HI - RAM_LO), "  }",
           "  /* the scratchpad and the I/O ports after it */",
           "  .psxscratch 0x1F800000 (NOLOAD) : { . = 0x4000; }",
           "}", "INSERT AFTER .bss;"]
    script = os.path.join(OUT, "psxram.ld")
    open(script, "w").write("\n".join(ld) + "\n")

    # symbols defined nowhere in C: the data of the executable, at its addresses
    undef = set()
    for o in objs:
        for line in run(["nm", "-u", o]).splitlines():
            undef.add(line.split()[-1])
    defined = set()
    for o in objs:
        for line in run(["nm", "--defined-only", o]).splitlines():
            p = line.split()
            if len(p) == 3 and p[1] in "TDBR" and re.match(r"^[A-Za-z_]\w*$", p[2]):
                defined.add(p[2])
    defsyms = []
    missing = []
    for s in sorted(undef - defined):
        if s in psx and (psx[s][1] in "DdBbRr" or psx[s][0] >= DATA_LO) and RAM_LO <= psx[s][0] < RAM_HI:
            defsyms.append("--defsym=%s=0x%08X" % (s, psx[s][0]))
        elif s in psx and psx[s][1] in "Aa":
            defsyms.append("--defsym=%s=0x%08X" % (s, psx[s][0]))
        else:
            missing.append(s)

    # the C functions by their PlayStation address, for pointers in the data
    funcs = sorted((psx[n][0], n) for n in defined if n in psx and psx[n][1] in "Tt")
    tab = ['#include "types.h"', ""]
    for _, n in funcs:
        tab.append("extern char %s[];" % n)
    tab += ["", "/* the C functions: PlayStation address, x86 address (sorted) */",
            "const u32 pc_func_table[][2] = {"]
    tab += ["    { 0x%08Xu, (u32)%s }," % (a, n) for a, n in funcs]
    tab += ["};", "const u32 pc_func_count = %d;" % len(funcs)]
    # where the startup code looks for pointers to C functions: the data of
    # the executable, and the C data placed before it (constant tables)
    ranges = [(DATA_LO, EXE_END)]
    ranges += [(a, a + sz) for a, sz, o, sec, n in placed if a < DATA_LO and not sec.startswith(".bss")]
    tab += ["", "const u32 pc_scan_ranges[][2] = {"]
    tab += ["    { 0x%08Xu, 0x%08Xu }," % r for r in ranges]
    tab += ["};", "const u32 pc_scan_count = %d;" % len(ranges)]
    open(os.path.join(OUT, "func_table.c"), "w").write("\n".join(tab) + "\n")
    run(CC + CFLAGS + ["-c", os.path.join(OUT, "func_table.c"), "-o", os.path.join(OUT, "func_table.o")])

    if missing:
        open(os.path.join(OUT, "missing.txt"), "w").write("\n".join(missing) + "\n")
        print("%d symbols are missing (build/pc/missing.txt): %s" % (len(missing), " ".join(missing[:30])))
    exe = os.path.join(OUT, "crash3")
    r = subprocess.run(CC + ["-no-pie", "-o", exe] + objs + [os.path.join(OUT, "func_table.o"),
                       "-Wl,-T," + script] + ["-Wl," + d for d in defsyms] + ["-lm"] + (SDL[1] if SDL else []),
                       capture_output=True, text=True)
    if r.returncode:
        errs = r.stderr.splitlines()
        print("\n".join(errs[:20]))
        sys.exit(1)
    print("%s: %d variables at their PlayStation addresses, %d data symbols of the executable, %d functions"
          % (exe, len(placed), len(defsyms), len(funcs)))
    if unplaced:
        print("variables without a PlayStation address: %s" % " ".join(n for _, _, n in unplaced[:20]))


main()
