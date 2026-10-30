#!/usr/bin/env python3
"""Known releases of Crash Bandicoot 3: Warped and their main executables.

The executable is the file named in SYSTEM.CNF (``BOOT = cdrom:\\SCUS_942.44;1``)
in the root of the game disc. Only ``us`` is the primary decompilation target;
the other entries are here so the tooling can recognise them later.

Usage: versions.py <version> <field>   (prints one field, used by the Makefile)
"""

import sys
from dataclasses import dataclass


@dataclass(frozen=True)
class Version:
    serial: str
    exe: str
    region: str


VERSIONS = {
    "us": Version(serial="SCUS-94244", exe="SCUS_942.44", region="NTSC-U"),
    "eu": Version(serial="SCES-01420", exe="SCES_014.20", region="PAL"),
    "jp": Version(serial="SCPS-10073", exe="SCPS_100.73", region="NTSC-J"),
}

DEFAULT_VERSION = "us"


def get(name: str) -> Version:
    try:
        return VERSIONS[name]
    except KeyError:
        known = ", ".join(VERSIONS)
        raise SystemExit(f"unknown version '{name}' (known: {known})")


def main(argv):
    if len(argv) != 3:
        raise SystemExit(__doc__.strip().splitlines()[-1])
    version = get(argv[1])
    field = argv[2]
    if field not in Version.__dataclass_fields__:
        raise SystemExit(f"unknown field '{field}'")
    print(getattr(version, field))


if __name__ == "__main__":
    main(sys.argv)
