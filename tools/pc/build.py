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

With --android it builds build/android/crash3.apk: the port as a
NativeActivity for 32-bit ARM (armeabi-v7a), cross-compiled with clang and
lld against the minimal interface of Android in tools/android (no NDK
needed), packed with aapt, zipalign and apksigner (a debug key).

usage: tools/pc/build.py [--windows | --android]
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
ANDROID = "--android" in sys.argv
# --linux-arm: the code of the Android build as a Linux program for ARM, to
# run in qemu-arm (tests of the port on ARM without a device)
LINUX_ARM = "--linux-arm" in sys.argv
OUT = os.path.join(ROOT, "build", "pc-win" if WINDOWS else "android" if ANDROID else "pc-arm" if LINUX_ARM else "pc")
MAIN_ELF = os.path.join(ROOT, "build", "us", "main.elf")
RC = os.path.join(ROOT, "build", "us", "rc")
ANDROID_DIR = os.path.join(ROOT, "tools", "android")
if WINDOWS:
    CC = ["i686-w64-mingw32-gcc"]
    NM = "i686-w64-mingw32-nm"
elif ANDROID:
    # 32-bit ARM as the NDK builds armeabi-v7a; position-independent (a
    # shared library); the C library interface of tools/android/sysroot
    # (GCC: the game code relies on its leniency, as with the other targets;
    # the ABI is that of armeabi-v7a: ARMv7-A, AAPCS with VFP and soft-float
    # calls)
    CC = ["arm-linux-gnueabi-gcc", "-march=armv7-a", "-mfloat-abi=softfp", "-mfpu=vfpv3-d16", "-marm", "-fPIC",
          "-D__ANDROID__", "-nostdinc",
          "-isystem", subprocess.run(["arm-linux-gnueabi-gcc", "-print-file-name=include"], capture_output=True,
                                     text=True).stdout.strip(),
          "-isystem", os.path.join(ANDROID_DIR, "sysroot", "include")]
    LD = ["clang", "--target=armv7a-linux-androideabi24", "-fuse-ld=lld"]
    NM = "llvm-nm"
elif LINUX_ARM:
    CC = ["arm-linux-gnueabi-gcc", "-march=armv7-a", "-mfloat-abi=softfp", "-mfpu=vfpv3-d16", "-marm", "-fPIC",
          "-DPSX_SCRATCH=0x80200000u"]
    NM = "llvm-nm"
else:
    CC = ["gcc", "-m32"]
    NM = "nm"
US = "_" if WINDOWS else ""  # the prefix of C symbols
# i686 without SSE: the variables are at their PlayStation addresses, which
# are not aligned as x86 compilers align arrays for vector instructions
# (-malign-double: 64-bit fields aligned as on MIPS; ARM does that anyway)
X86 = ["-march=i686", "-mno-sse", "-malign-double", "-fno-pic"]
CFLAGS = ([] if ANDROID or LINUX_ARM else X86) + \
    ["-std=gnu99", "-O1", "-g", "-fno-common", "-fno-strict-aliasing", "-fwrapv", "-funsigned-char",
     "-fno-stack-protector", "-fno-unwind-tables", "-fno-asynchronous-unwind-tables",
     "-DNON_MATCHING", "-DVERSION_US", "-DTARGET_PC", "-I" + os.path.join(ROOT, "include"), "-w"]
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
# the overlay (code loaded at run time) and its functions the C code calls
# (names other than func_*: declared as functions in src/sys.c)
OVERLAY_LO, OVERLAY_HI = 0x8006EA78, 0x80072230
OVERLAY_FUNCS = {"D_8006F64C", "D_8006F780"}
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
    if ANDROID:
        return None  # the window and the input are src/pc/android.c
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
        if f.endswith(".c") and (ANDROID or f != "android.c"):
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


# The libraries of Android the port links with, and what it may use from
# them: the stubs the link sees are made from these lists (a name not here is
# an error: it might not exist on the device).
ANDROID_LIBS = {
    "libandroid.so": r"^(ANativeWindow_|AInputQueue_|AInputEvent_|AKeyEvent_|AMotionEvent_|ALooper_|ANativeActivity_)",
    "liblog.so": r"^__android_log_",
    "libc.so": r"^(strtoul|fopen|freopen|fclose|fread|fwrite|fseek|ftell|fgets|fputc|fputs|fflush|setvbuf|printf|fprintf|"
               r"vfprintf|sprintf|snprintf|vsnprintf|puts|malloc|calloc|realloc|free|exit|abort|atoi|getenv|memcpy|"
               r"memmove|memset|memcmp|strlen|strcmp|strncmp|strchr|strrchr|strstr|strcpy|strncpy|strcat|strspn|"
               r"strerror|strcasecmp|strncasecmp|__errno|opendir|readdir|closedir|clock_gettime|nanosleep|"
               r"setitimer|sigaction|sigprocmask|pthread_sigmask|sigemptyset|sigaddset|mmap|munmap|usleep|"
               r"raise|pthread_attr_init|pthread_attr_setstacksize|pthread_create|pthread_kill|pthread_self|"
               r"__aeabi_memcpy\w*|__aeabi_memset\w*|__aeabi_memclr\w*|__aeabi_memmove\w*|stdin|stdout|stderr)$",
    "libm.so": r"^(sqrt|sqrtf|sin|cos|atan2|floor|ceil|pow|fabs)$",
}
ANDROID_DATA = {"stdin", "stdout", "stderr"}


def link_android(objs, defsyms):
    """libcrash3.so with stubs of the system libraries, then the APK."""
    libgcc = run(["arm-linux-gnueabi-gcc", "-print-libgcc-file-name"]).strip()
    so = os.path.join(OUT, "lib", "armeabi-v7a", "libcrash3.so")
    os.makedirs(os.path.dirname(so), exist_ok=True)
    # what the objects need from the system (libgcc: the division helpers)
    undef, defined = set(), set()
    for o in objs + [os.path.join(OUT, "func_table.o")]:
        undef |= set(n for t, n in symbols(o, "-u"))
        defined |= set(n for t, n in symbols(o, "--defined-only"))
    for t, n in symbols(libgcc, "--defined-only"):
        defined.add(n)
    absolute = set(d.split("=")[1] for d in defsyms)
    # and what libgcc may need (raise: division by zero)
    undef |= {"raise"}
    need = sorted(undef - defined - absolute)
    stubs = os.path.join(OUT, "stubs")
    os.makedirs(stubs, exist_ok=True)
    per_lib = dict((lib, []) for lib in ANDROID_LIBS)
    unknown = []
    for n in need:
        lib = next((l for l, rx in ANDROID_LIBS.items() if re.match(rx, n)), None)
        if lib:
            per_lib[lib].append(n)
        elif n not in ("__aeabi_unwind_cpp_pr0", "_GLOBAL_OFFSET_TABLE_"):  # made by the linker
            unknown.append(n)
    if unknown:
        sys.exit("not in the libraries of Android the port links with: %s" % " ".join(unknown))
    for lib, names in per_lib.items():
        c = os.path.join(stubs, lib + ".c")
        open(c, "w").write("".join(("void *%s;\n" if n in ANDROID_DATA else "void %s(void) {}\n") % n
                                   for n in names))
        run(LD + ["-shared", "-nostdlib", "-Wl,-soname," + lib, "-o", os.path.join(stubs, lib), c])
    # only the entry of the activity is exported: the addresses of the
    # PlayStation (absolute symbols) are then resolved by the link. Exported,
    # they would be bound at load time, and the loader of Android adds the
    # load address of the library to absolute symbols.
    script = os.path.join(OUT, "exports.map")
    open(script, "w").write("{ global: ANativeActivity_onCreate; local: *; };\n")
    link = LD + ["-shared", "-nostdlib", "-o", so, "-Wl,-soname,libcrash3.so", "-Wl,--version-script=" + script,
                 "-Wl,--no-undefined", "-Wl,-z,noexecstack", "-Wl,-z,max-page-size=16384", "-Wl,--hash-style=both"] + \
        objs + [os.path.join(OUT, "func_table.o")] + ["-Wl," + d for d in defsyms] + [libgcc] + \
        [os.path.join(stubs, lib) for lib in ANDROID_LIBS]
    r = subprocess.run(link, capture_output=True, text=True)
    if r.returncode:
        print("\n".join(r.stderr.splitlines()[:30]))
        sys.exit(1)
    exported = [l.split()[-1] for l in run(["llvm-readelf", "--dyn-syms", "-W", so]).splitlines()
                if len(l.split()) >= 8 and l.split()[6] != "UND" and l.split()[0].endswith(":")
                and l.split()[0][:-1].isdigit() and l.split()[-1] != "ANativeActivity_onCreate"
                and l.split()[3] != "SECTION" and l.split()[1] != "00000000"]
    if exported:
        sys.exit("libcrash3.so exports more than its entry: %s" % " ".join(exported[:10]))
    # and no relocation at load time may touch a word that holds a
    # PlayStation address (the loader would add the load address)
    import struct as st
    data = open(so, "rb").read()
    segs = []  # (vaddr, offset, filesz)
    for l in run(["llvm-readelf", "-lW", so]).splitlines():
        p = l.split()
        if p and p[0] == "LOAD":
            segs.append((int(p[2], 16), int(p[1], 16), int(p[4], 16)))
    bad = 0
    for l in run(["llvm-readelf", "-rW", so]).splitlines():
        p = l.split()
        if len(p) >= 3 and p[2] == "R_ARM_RELATIVE":
            off = int(p[0], 16)
            for va, fo, fs in segs:
                if va <= off < va + fs:
                    w = st.unpack_from("<I", data, fo + off - va)[0]
                    bad += 0x80000000 <= w < 0x80220000 or 0x1F800000 <= w < 0x1F804000
    if bad:
        sys.exit("%d relocations at load time would move PlayStation addresses" % bad)
    return package_apk(so)


def package_apk(so):
    jar = "/usr/lib/android-sdk/platforms/android-23/android.jar"
    apk = os.path.join(OUT, "crash3.apk")
    tmp = os.path.join(OUT, "unsigned.apk")
    aligned = os.path.join(OUT, "aligned.apk")
    for f in (tmp, aligned, apk):
        if os.path.exists(f):
            os.remove(f)
    run(["aapt", "package", "-f", "-M", os.path.join(ANDROID_DIR, "AndroidManifest.xml"), "-I", jar, "-F", tmp])
    run(["aapt", "add", tmp, "lib/armeabi-v7a/libcrash3.so"], cwd=OUT)
    run(["zipalign", "-f", "-p", "4", tmp, aligned])
    # a debug key (made once; keep build/android-key.jks to update the app)
    key = os.path.join(ROOT, "build", "android-key.jks")
    if not os.path.exists(key):
        run(["keytool", "-genkeypair", "-keystore", key, "-storepass", "android", "-keypass", "android",
             "-alias", "crash3", "-keyalg", "RSA", "-keysize", "2048", "-validity", "10000",
             "-dname", "CN=Crash 3 port (debug)"])
    run(["apksigner", "sign", "--ks", key, "--ks-pass", "pass:android", "--key-pass", "pass:android",
         "--ks-key-alias", "crash3", "--out", apk, aligned])
    os.remove(tmp)
    os.remove(aligned)
    return apk


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
    # the functions of the overlay the game loads at run time (S0/WARPSCUS.BIN,
    # between the end of .bss and D_80072230): C thunks that interpret them
    # (src/pc/mips.c)
    overlay = sorted(n for n in undef - defined if n in psx and OVERLAY_LO <= psx[n][0] < OVERLAY_HI
                     and (n.startswith("func_") or n in OVERLAY_FUNCS))
    defsyms, missing = [], []
    for s in sorted(undef - defined - set(overlay)):
        if is_data(psx, s) or (s in psx and psx[s][1] in "Aa"):  # A: constants like D_40100
            a = psx[s][0]
            if (ANDROID or LINUX_ARM) and 0x1F800000 <= a < 0x1F804000:
                a += 0x80200000 - 0x1F800000  # the scratchpad of Android (PSX_SCRATCH)
            defsyms.append("--defsym=%s%s=0x%08X" % (US, s, a))
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
    tab += ["", "/* the functions of the overlay: interpreted */",
            "u32 pc_mips_call(u32 addr, u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5, u32 a6, u32 a7);"]
    for n in overlay:
        tab += ["u32 %s(u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5, u32 a6, u32 a7)" % n,
                "{", "    return pc_mips_call(0x%08Xu, a0, a1, a2, a3, a4, a5, a6, a7);" % psx[n][0], "}"]
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
    if ANDROID:
        exe = link_android(objs, defsyms)
    else:
        if WINDOWS:
            exe = os.path.join(OUT, "crash3.exe")
            # 4 GB of addresses for a 32-bit program on 64-bit Windows: the RAM
            # is at 0x80000000
            link = CC + ["-o", exe] + objs + [os.path.join(OUT, "func_table.o"), "-Wl,--large-address-aware"] + \
                ["-Wl," + d for d in defsyms] + (SDL[1] if SDL else []) + ["-lwinmm"]
        elif LINUX_ARM:
            exe = os.path.join(OUT, "crash3")
            link = CC + ["-no-pie", "-o", exe] + objs + [os.path.join(OUT, "func_table.o")] + \
                ["-Wl," + d for d in defsyms] + ["-lm", "-lpthread"]
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
