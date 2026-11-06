"""Builds the PC port (32-bit x86): build/pc/crash3 (Linux) or, with
--windows, build/pc-win/crash3.exe and SDL2.dll (cross-compiled with
MinGW-w64, e.g. in WSL; it runs on 64-bit Windows).

The memory model of the port: the 2 MB of PlayStation RAM are at their own
address, 0x80000000, in the 32-bit process, and the scratchpad at 0x1F800000
(src/pc/startup.c maps them and loads the user's executable there). So the
data of the game that is not C (the data segment of the executable: tables,
pointers between them) is used in place, and the global variables of the C
files are used at the addresses they have in the PlayStation build
(build/us/main.elf): pointers in the data to them stay right, and their
initial values come from the executable.

For that the game files are compiled to assembly, where the definition of
each such variable is renamed: the code refers to it by its name, which the
link turns into its PlayStation address (as for the symbols of the data
segment). A table of the C functions (PlayStation address -> x86 address) lets
the startup code replace pointers to functions in the data.

usage: tools/pc/build.py [--windows]
"""
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tarfile
import urllib.request
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
WINDOWS = "--windows" in sys.argv
OUT = os.path.join(ROOT, "build", "pc-win" if WINDOWS else "pc")
MAIN_ELF = os.path.join(ROOT, "build", "us", "main.elf")
RC = os.path.join(ROOT, "build", "us", "rc")
CC = ["i686-w64-mingw32-gcc"] if WINDOWS else ["gcc", "-m32"]
NM = "i686-w64-mingw32-nm" if WINDOWS else "nm"
US = "_" if WINDOWS else ""  # the prefix of C symbols
# i686 without SSE: the variables are at their PlayStation addresses, which
# are not aligned as x86 compilers align arrays for vector instructions
CFLAGS = ["-march=i686", "-mno-sse", "-std=gnu99", "-O1", "-g", "-fno-pic", "-fno-common", "-fno-strict-aliasing",
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
# the constant data before the code (the .rodata of the C files and the
# segment "1080" in config/us/splat.yaml), and the data segment (file offset
# 0x4C440): symbols there are data even when the assembly typed them as text
RODATA_LO, RODATA_HI = 0x80010000, 0x8001101C
DATA_LO = 0x8005BC40
EXE_END = 0x80061000  # the end of what the executable loads
# SDL2 for MinGW (the Windows build)
SDL_VERSION = "2.30.9"
SDL_URL = "https://github.com/libsdl-org/SDL/releases/download/release-%s/SDL2-devel-%s-mingw.tar.gz" % (
    SDL_VERSION, SDL_VERSION)
SDL_SHA256 = "b188d165dc4a0372042bff3d0c18e9bb90772581e92a7b8b47b521b47714f75c"


def run(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode:
        sys.stderr.write(" ".join(cmd) + "\n" + r.stdout + r.stderr)
        sys.exit(1)
    return r.stdout


def sdl_flags():
    """(cflags, libs, dll) of a 32-bit SDL2, or None: without it the port
    runs without a window (frames can be written to files)."""
    if WINDOWS:
        base = os.path.join(ROOT, "build", "sdl2-mingw")
        dev = os.path.join(base, "SDL2-%s" % SDL_VERSION, "i686-w64-mingw32")
        if not os.path.isdir(dev):
            os.makedirs(base, exist_ok=True)
            archive = os.path.join(base, "sdl.tar.gz")
            print("downloading SDL2 %s for MinGW" % SDL_VERSION)
            urllib.request.urlretrieve(SDL_URL, archive)
            if hashlib.sha256(open(archive, "rb").read()).hexdigest() != SDL_SHA256:
                sys.exit("the SDL2 archive has a wrong checksum")
            with tarfile.open(archive) as t:
                t.extractall(base)
        return (["-I" + os.path.join(dev, "include", "SDL2")],
                ["-L" + os.path.join(dev, "lib"), "-lSDL2"], os.path.join(dev, "bin", "SDL2.dll"))
    env = dict(os.environ, PKG_CONFIG_PATH="/usr/lib/i386-linux-gnu/pkgconfig")
    r = subprocess.run(["pkg-config", "--cflags", "--libs", "sdl2"], capture_output=True, text=True, env=env)
    if r.returncode:
        return None
    c = subprocess.run(["pkg-config", "--cflags", "sdl2"], capture_output=True, text=True, env=env).stdout.split()
    l = subprocess.run(["pkg-config", "--libs", "sdl2"], capture_output=True, text=True, env=env).stdout.split()
    return c, l, None


SDL = sdl_flags()


def psx_symbols():
    """name -> (address, type) of the PlayStation build."""
    syms = {}
    for line in run(["mipsel-linux-gnu-nm", MAIN_ELF]).splitlines():
        p = line.split()
        if len(p) == 3:
            syms.setdefault(p[2], (int(p[0], 16), p[1]))
    return syms


def is_data(psx, name):
    """Whether name is a variable of the PlayStation build in the RAM."""
    if name not in psx:
        return False
    a, t = psx[name]
    if not RAM_LO <= a < RAM_HI:
        return False
    return t in "DdBbRrAa" or a >= DATA_LO or RODATA_LO <= a < RODATA_HI


DIRECTIVES = (".globl", ".global", ".type", ".size", ".local", ".weak", ".def", ".comm", ".lcomm")


def move_variables(asm, psx, moved):
    """Renames the definitions of the variables that live at their
    PlayStation addresses in the assembly of a game file: the references
    stay, and the link gives them the addresses. moved: the names (added)."""
    lines = asm.splitlines()
    names = set()  # the labels of variables there (in data sections)
    in_data = False
    for line in lines:
        s = line.strip()
        m = re.match(r"^\.(text|data|bss|section\s+([^,\s]+))", s)
        if m:
            sec = m.group(2) or "." + m.group(1)
            in_data = not sec.startswith(".text")
        m = re.match(r"^%s(\w+):$" % US, s)
        if m and in_data and is_data(psx, m.group(1)):
            names.add(m.group(1))
        # uninitialized statics: .local + .comm (ELF), .lcomm (MinGW)
        m = re.match(r"^\.(?:comm|lcomm)\s+%s(\w+)\s*," % US, s)
        if m and is_data(psx, m.group(1)):
            names.add(m.group(1))
    moved |= names
    out = []
    for line in lines:
        s = line.strip()
        m = re.match(r"^%s(\w+):$" % US, s)
        if m and m.group(1) in names:
            line = line.replace(s, "%s%s__pc_moved:" % (US, m.group(1)))
        elif s.startswith(DIRECTIVES):
            for name in re.findall(r"\b%s(\w+)" % US, s):
                if name in names:
                    line = re.sub(r"\b%s%s\b" % (US, name), "%s%s__pc_moved" % (US, name), line)
        out.append(line)
    return "\n".join(out) + "\n"


def compile_all(psx):
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
    moved = set()

    def one(job):
        src, obj = job
        game = "/src/pc/" not in src and "/rc/" not in src
        extra = ["-Dmain=game_main"] if src.endswith("/src/main.c") else []
        if (src.endswith("/src/pc/video.c") or src.endswith("/src/pc/pad.c")) and SDL:
            extra += ["-DPC_SDL"] + SDL[0]
        if game:
            extra += HEAP_NAMES
            asm = run(CC + CFLAGS + extra + ["-S", src, "-o", "-"])
            s = obj[:-2] + ".s"
            open(s, "w").write(move_variables(asm, psx, moved))
            run(CC + ["-Wa,-I" + os.path.join(ROOT, "include"), "-c", s, "-o", obj], cwd=ROOT)
            return
        if os.path.exists(obj) and os.path.getmtime(obj) > max(os.path.getmtime(src), newest_header()):
            return
        run(CC + CFLAGS + extra + ["-c", src, "-o", obj])

    with ThreadPoolExecutor(os.cpu_count()) as ex:
        list(ex.map(one, jobs))
    return [o for _, o in jobs], moved


def newest_header():
    inc = os.path.join(ROOT, "include")
    return max(os.path.getmtime(os.path.join(inc, f)) for f in os.listdir(inc))


def symbols(obj, flag):
    out = []
    for line in run([NM, flag, obj]).splitlines():
        p = line.split()
        if p:
            out.append((p[-2] if len(p) > 1 else "U", p[-1]))
    return out


def unprefix(n):
    return n[len(US):] if US and n.startswith(US) else n


def main():
    psx = psx_symbols()
    objs, moved = compile_all(psx)

    # symbols defined nowhere: the variables at their PlayStation addresses
    # and the data of the executable
    undef, defined = set(), set()
    for o in objs:
        undef |= set(unprefix(n) for t, n in symbols(o, "-u"))
        defined |= set(unprefix(n) for t, n in symbols(o, "--defined-only")
                       if t in "TDBR" and re.match(r"^%s[A-Za-z_]\w*$" % US, n))
    defsyms, missing = [], []
    for s in sorted(undef - defined):
        if is_data(psx, s) or (s in psx and psx[s][1] in "Aa"):  # A: constants like D_40100
            defsyms.append("--defsym=%s%s=0x%08X" % (US, s, psx[s][0]))
        else:
            missing.append(s)

    # the C functions by their PlayStation address, for pointers in the data
    funcs = sorted((psx[n][0], n) for n in defined if n in psx and psx[n][1] in "Tt" and not is_data(psx, n))
    tab = ['#include "types.h"', ""]
    for _, n in funcs:
        tab.append("extern char %s[];" % n)
    tab += ["", "/* the C functions: PlayStation address, x86 address (sorted) */",
            "const u32 pc_func_table[][2] = {"]
    tab += ["    { 0x%08Xu, (u32)%s }," % (a, n) for a, n in funcs]
    tab += ["};", "const u32 pc_func_count = %d;" % len(funcs)]
    # where the startup code looks for pointers to C functions: the constant
    # data before the code and the data segment
    ranges = [(RODATA_LO, RODATA_HI), (DATA_LO, EXE_END)]
    tab += ["", "const u32 pc_scan_ranges[][2] = {"]
    tab += ["    { 0x%08Xu, 0x%08Xu }," % r for r in ranges]
    tab += ["};", "const u32 pc_scan_count = %d;" % len(ranges)]
    open(os.path.join(OUT, "func_table.c"), "w").write("\n".join(tab) + "\n")
    run(CC + CFLAGS + ["-c", os.path.join(OUT, "func_table.c"), "-o", os.path.join(OUT, "func_table.o")])

    if missing:
        open(os.path.join(OUT, "missing.txt"), "w").write("\n".join(missing) + "\n")
    if WINDOWS:
        exe = os.path.join(OUT, "crash3.exe")
        # 4 GB of addresses for a 32-bit program on 64-bit Windows: the RAM is
        # at 0x80000000
        link = CC + ["-o", exe] + objs + [os.path.join(OUT, "func_table.o"), "-Wl,--large-address-aware"] + \
            ["-Wl," + d for d in defsyms] + (SDL[1] if SDL else []) + ["-lwinmm"]
    else:
        exe = os.path.join(OUT, "crash3")
        link = CC + ["-no-pie", "-o", exe] + objs + [os.path.join(OUT, "func_table.o")] + \
            ["-Wl," + d for d in defsyms] + ["-lm"] + (SDL[1] if SDL else [])
    r = subprocess.run(link, capture_output=True, text=True)
    if r.returncode:
        print("\n".join(r.stderr.splitlines()[:20]))
        sys.exit(1)
    if WINDOWS and SDL:
        shutil.copy(SDL[2], OUT)
    print("%s: %d variables at their PlayStation addresses, %d symbols of the executable, %d functions"
          % (exe, len(moved), len(defsyms) - len(moved), len(funcs)))


main()
