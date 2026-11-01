#!/usr/bin/env python3
"""Writes a linker script that keeps every segment at its original address.

    pad_ld.py splat.yaml main.ld out.ld

For NON_MATCHING builds: code compiled from C that is not byte-identical is
usually a little shorter or longer, which moves everything after it. The
hand-written assembly and data still refer to some addresses as plain
numbers, so the game would break. Here each segment that has a start offset in
splat.yaml starts at its original address again (the gap is filled with
zeros); a segment that grew past the next one's start is left where it is.
"""
import re
import sys

ROM_TO_VRAM = 0x80010000 - 0x800

KIND = {
    "c": "text", "asm": "text", "hasm": "text",
    "rodata": "rodata", ".rodata": "rodata",
    "data": "data", ".data": "data",
    "sdata": "sdata", ".sdata": "sdata",
}


def main():
    yaml_path, ld_in, ld_out = sys.argv[1:4]
    starts = {}
    for line in open(yaml_path):
        m = re.match(r'\s*-\s*\[(0x[0-9A-Fa-f]+),\s*([.\w]+),\s*"?([\w.]+)"?\]', line)
        if not m or m.group(2) not in KIND:
            continue
        starts[(m.group(3), KIND[m.group(2)])] = int(m.group(1), 16) + ROM_TO_VRAM

    out = []
    section = None
    base = 0
    for line in open(ld_in):
        m = re.match(r'\s*(\.\w+)\s+(0x[0-9A-Fa-f]+\s+)?.*:\s*AT\(', line)
        if m:
            section = m.group(1)
            if m.group(2):
                base = int(m.group(2), 16)
        m = re.match(r'(\s*)\S+/([\w.]+?)\.(?:c|s)\.o\(\.(\w+)\);', line)
        if m and section == ".main":
            key = (m.group(2), m.group(3))
            if key in starts:
                out.append("%s. = MAX(., 0x%X);\n" % (m.group(1), starts[key] - base))
        out.append(line)
    open(ld_out, "w").write("".join(out))


main()
