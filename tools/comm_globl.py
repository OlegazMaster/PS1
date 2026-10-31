#!/usr/bin/env python3
"""Keeps uninitialized globals global through maspsx.

GCC emits a non-static uninitialized variable as `.comm name,size`. maspsx
allocates small ones in .sbss like ASPSX did, but leaves their labels local,
so other files could not refer to them. This filter, run between cc1 and
maspsx, declares each such symbol `.globl` first. Static variables use
`.lcomm` and stay local.
"""

import re
import sys

COMM = re.compile(r"^\s*\.comm\s+([A-Za-z_.$][\w.$]*)\s*,")


def main():
    for line in sys.stdin:
        match = COMM.match(line)
        if match:
            sys.stdout.write(f"\t.globl\t{match.group(1)}\n")
        sys.stdout.write(line)


if __name__ == "__main__":
    main()
