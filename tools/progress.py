#!/usr/bin/env python3
"""Reports how much of the game code has been decompiled to C.

Only what the splat linker script links is counted, so asm left over from
earlier splits is ignored. A function of a c file counts as decompiled once the
file no longer has an INCLUDE_ASM for it; functions in whole-file asm segments
count as not decompiled. Sizes are in bytes of MIPS code.

Usage: progress.py [--version us] [--verbose]
"""

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

import versions

ROOT = Path(__file__).resolve().parent.parent

# "/* 1234 80012345 E8FFBD27 */  addiu $sp, $sp, -0x18" as written by splat.
INSTRUCTION = re.compile(r"^\s*/\*\s*[0-9A-Fa-f]+\s+[0-9A-Fa-f]{8}\s+[0-9A-Fa-f]{8}\s*\*/\s+[a-z]")
FUNCTION = re.compile(r"^\s*glabel\s+(\w+)")
SECTION = re.compile(r"^\s*\.section\s+([.\w]+)")
INCLUDE_ASM = re.compile(r"^\s*INCLUDE_ASM\(\s*\"[^\"]*\"\s*,\s*(\w+)\s*\)", re.MULTILINE)
# "build/us/src/gool.c.o(.text);" in the linker script -> "src/gool.c".
LINKED_OBJECT = re.compile(r"\bbuild/[^/\s]+/(\S+?)\.o\b")

# Code that is reported separately from the game's own C code, by file name.
GAME = "код игры"
CATEGORIES = {
    "nd_asm": "ассемблер Naughty Dog",
    "psyq": "библиотеки PSY-Q",
}


@dataclass
class Stats:
    done_bytes: int = 0
    total_bytes: int = 0
    done_funcs: int = 0
    total_funcs: int = 0
    remaining: dict = field(default_factory=dict)  # function name -> size

    def add(self, name: str, size: int, done: bool):
        self.total_bytes += size
        self.total_funcs += 1
        if done:
            self.done_bytes += size
            self.done_funcs += 1
        else:
            self.remaining[name] = size


def function_sizes(asm_file: Path) -> dict:
    """Returns {function name: code size} for every function in an asm file."""
    sizes = {}
    current = None
    in_text = True
    for line in asm_file.read_text(encoding="utf-8", errors="replace").splitlines():
        section = SECTION.match(line)
        if section:
            in_text = section.group(1).startswith(".text")
            continue
        function = FUNCTION.match(line)
        if function and in_text:
            current = function.group(1)
            sizes[current] = 0
        elif current and in_text and INSTRUCTION.match(line):
            sizes[current] += 4
    return sizes


def included_functions(c_file: Path) -> set:
    if not c_file.exists():
        return set()
    return set(INCLUDE_ASM.findall(c_file.read_text(encoding="utf-8", errors="replace")))


def linked_sources(ld_script: Path) -> list:
    """Source files (relative to the repository) in link order, without duplicates."""
    sources = LINKED_OBJECT.findall(ld_script.read_text(encoding="utf-8"))
    return list(dict.fromkeys(sources))


def category(source: str) -> str:
    for part in Path(source).with_suffix("").parts:
        for prefix, name in CATEGORIES.items():
            if part.startswith(prefix):
                return name
    return GAME


def scan(root: Path, version: str) -> dict:
    """Returns {category: Stats}."""
    all_stats = {GAME: Stats()}
    all_stats.update({name: Stats() for name in CATEGORIES.values()})
    asm_dir = root / "asm" / version
    for source in linked_sources(asm_dir / "main.ld"):
        stats = all_stats[category(source)]
        if source.endswith(".c"):
            # splat writes the asm of the functions of src/<name>.c to
            # asm/<version>/nonmatchings/<name>/ (still INCLUDE_ASM) and
            # asm/<version>/matchings/<name>/ (already in C).
            still_asm = included_functions(root / source)
            name = Path(source).relative_to("src").with_suffix("")
            functions = {}
            for kind in ("nonmatchings", "matchings"):
                for asm_file in sorted((asm_dir / kind / name).glob("*.s")):
                    functions.update(function_sizes(asm_file))
            for function, size in functions.items():
                if size:
                    stats.add(function, size, function not in still_asm)
        elif source.endswith(".s"):
            for function, size in function_sizes(root / source).items():
                if size:
                    stats.add(function, size, False)
    return all_stats


def percent(part: int, whole: int) -> str:
    return f"{100.0 * part / whole:.2f}%" if whole else "0.00%"


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.strip().splitlines()[0])
    parser.add_argument("--version", default=versions.DEFAULT_VERSION)
    parser.add_argument("--verbose", action="store_true", help="list functions left to do")
    args = parser.parse_args(argv[1:])

    ld_script = ROOT / "asm" / args.version / "main.ld"
    if not ld_script.exists():
        raise SystemExit(f"{ld_script.relative_to(ROOT)} не найден: сначала выполните 'make extract'")

    print(f"Crash Bandicoot 3 ({args.version}), декомпилировано:")
    for name, stats in scan(ROOT, args.version).items():
        print(f"  {name:24s} {stats.done_bytes:7d}/{stats.total_bytes:<7d} байт "
              f"({percent(stats.done_bytes, stats.total_bytes):>7s}), "
              f"функций {stats.done_funcs}/{stats.total_funcs}")
        if args.verbose:
            for function, size in sorted(stats.remaining.items(), key=lambda item: item[1]):
                print(f"      {size:6d}  {function}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
