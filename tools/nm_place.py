#!/usr/bin/env python3
"""Keeps every function of a C file at its original address (NON_MATCHING builds).

    ... | maspsx | nm_place.py <module> <too_long.txt> | as

The overlays on the disc (for example the warp room, S0/WARPSCUS.BIN) are
linked against the original executable and call its functions at fixed
addresses, so in a NON_MATCHING build every function must stay where it was.
Each function gets a `.org` to its original offset in the file (a shorter
function leaves zeros after it). The functions listed in too_long.txt, whose
C code is longer than the original, are replaced by their original assembly.
"""
import glob
import re
import sys

mod, too_long_path = sys.argv[1], sys.argv[2]

addr = {}
for path in glob.glob("asm/us/nonmatchings/%s/*.s" % mod) + glob.glob("asm/us/matchings/%s/*.s" % mod):
    text = open(path).read()
    g = re.search(r"^glabel (\w+)\s*$", text, re.M)
    v = re.search(r"/\* [0-9A-F]+ ([0-9A-F]{8}) [0-9A-F]{8} \*/", text[g.end():]) if g else None
    if g and v:
        addr[g.group(1)] = int(v.group(1), 16)
start = min(addr.values()) if addr else 0

too_long = set()
try:
    for line in open(too_long_path):
        line = line.split("#")[0].strip()
        if line:
            too_long.add(line)
except FileNotFoundError:
    pass

out = []
skip = None
for line in sys.stdin:
    if skip:
        if re.match(r"\s*\.end\s+%s\s*$" % re.escape(skip), line):
            out.append(".set noat\n.set noreorder\n")
            out.append('.include "asm/us/nonmatchings/%s/%s.s"\n' % (mod, skip))
            out.append(".set reorder\n.set at\n")
            skip = None
        continue
    m = re.match(r"\s*\.ent\s+(\w+)\s*$", line)
    if m and m.group(1) in addr:
        name = m.group(1)
        out.append("\t.org 0x%X\n" % (addr[name] - start))
        if name in too_long:
            skip = name
            continue
    m = re.match(r'\s*\.include "asm/us/nonmatchings/%s/(\w+)\.s"' % re.escape(mod), line)
    if m and m.group(1) in addr:
        out.append("\t.org 0x%X\n" % (addr[m.group(1)] - start))
    out.append(line)
sys.stdout.write("".join(out))
