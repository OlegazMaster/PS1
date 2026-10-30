#!/usr/bin/env python3
"""Extracts files from a PlayStation CD image (.cue/.bin, raw .bin or .iso).

Without --extract, copies the boot executable named in SYSTEM.CNF into disc/.
Only the ISO 9660 data track is read; a partial image works as long as it
contains the requested file.

Usage: disc_image.py <image> [--list] [--extract PATH] [-o OUTPUT]
"""

import argparse
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

RAW_SECTOR_SIZE = 2352
SECTOR_SIZE = 2048
SYNC = bytes([0x00] + [0xFF] * 10 + [0x00])
PVD_LBA = 16
DIR_FLAG = 0x02
BOOT_LINE = re.compile(r"^\s*BOOT\s*=\s*cdrom:\\*([^\s;]+)", re.IGNORECASE | re.MULTILINE)


class DiscError(Exception):
    pass


class DiscImage:
    def __init__(self, data: bytes):
        self.data = data
        self.raw = data[:len(SYNC)] == SYNC
        if not self.raw and len(data) < (PVD_LBA + 1) * SECTOR_SIZE:
            raise DiscError("файл слишком мал для образа диска")

        pvd = self.sector(PVD_LBA)
        if pvd[0] != 1 or pvd[1:6] != b"CD001":
            raise DiscError("файловая система ISO 9660 не найдена")
        self.root = self.parse_record(pvd[156:190])

    @property
    def sector_count(self) -> int:
        return len(self.data) // (RAW_SECTOR_SIZE if self.raw else SECTOR_SIZE)

    def sector(self, lba: int) -> bytes:
        if lba >= self.sector_count:
            raise DiscError(f"сектор {lba} за пределами образа ({self.sector_count} секторов)")
        if not self.raw:
            return self.data[lba * SECTOR_SIZE:(lba + 1) * SECTOR_SIZE]
        start = lba * RAW_SECTOR_SIZE
        mode = self.data[start + 15]
        # Mode 1: 16 bytes of sync+header; mode 2 form 1 adds an 8-byte subheader.
        offset = start + (16 if mode == 1 else 24)
        return self.data[offset:offset + SECTOR_SIZE]

    def read(self, lba: int, size: int) -> bytes:
        count = (size + SECTOR_SIZE - 1) // SECTOR_SIZE
        return b"".join(self.sector(lba + i) for i in range(count))[:size]

    @staticmethod
    def parse_record(record: bytes) -> dict:
        lba = struct.unpack_from("<I", record, 2)[0]
        size = struct.unpack_from("<I", record, 10)[0]
        name_len = record[32]
        name = record[33:33 + name_len].decode("ascii", "replace")
        return {"name": name.split(";")[0], "lba": lba, "size": size,
                "is_dir": bool(record[25] & DIR_FLAG)}

    def list_dir(self, directory: dict) -> list:
        entries = []
        data = self.read(directory["lba"], directory["size"])
        pos = 0
        while pos < len(data):
            length = data[pos]
            if length == 0:
                # Records never cross a sector boundary: continue in the next sector.
                pos = (pos // SECTOR_SIZE + 1) * SECTOR_SIZE
                continue
            entry = self.parse_record(data[pos:pos + length])
            if entry["name"] not in ("\0", "\1"):  # "." and ".."
                entries.append(entry)
            pos += length
        return entries

    def find(self, path: str) -> dict:
        entry = self.root
        for part in re.split(r"[\\/]+", path.strip("\\/")):
            matches = [e for e in self.list_dir(entry) if e["name"].upper() == part.upper()]
            if not matches:
                raise DiscError(f"на диске нет {path}")
            entry = matches[0]
        return entry

    def read_file(self, path: str) -> bytes:
        entry = self.find(path)
        return self.read(entry["lba"], entry["size"])

    def boot_executable(self) -> str:
        system_cnf = self.read_file("SYSTEM.CNF").decode("ascii", "replace")
        match = BOOT_LINE.search(system_cnf)
        if not match:
            raise DiscError("в SYSTEM.CNF нет строки BOOT")
        return match.group(1)

    def walk(self, directory=None, prefix=""):
        for entry in self.list_dir(directory or self.root):
            path = prefix + entry["name"]
            yield path, entry
            if entry["is_dir"]:
                try:
                    yield from self.walk(entry, path + "/")
                except DiscError:
                    pass  # directory lies outside of a partial image


def load_image(path: Path) -> bytes:
    if path.suffix.lower() == ".cue":
        match = re.search(r'^\s*FILE\s+"([^"]+)"', path.read_text(errors="replace"), re.MULTILINE)
        if not match:
            raise DiscError(f"{path}: в .cue нет строки FILE")
        path = path.parent / match.group(1)
    return path.read_bytes()


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.strip().splitlines()[0])
    parser.add_argument("image", type=Path)
    parser.add_argument("--list", action="store_true", help="list all files")
    parser.add_argument("--extract", metavar="PATH", help="file to extract (default: boot executable)")
    parser.add_argument("-o", "--output", type=Path, help="default: disc/<file name>")
    args = parser.parse_args(argv[1:])

    try:
        disc = DiscImage(load_image(args.image))
        if args.list:
            for path, entry in disc.walk():
                print(f"{entry['lba']:7d} {entry['size']:10d}  {path}{'/' if entry['is_dir'] else ''}")
            return 0
        name = args.extract or disc.boot_executable()
        data = disc.read_file(name)
    except (OSError, DiscError) as e:
        raise SystemExit(f"ошибка: {e}")

    output = args.output or ROOT / "disc" / Path(name.replace("\\", "/")).name
    output.write_bytes(data)
    print(f"{name}: {len(data)} байт -> {output}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
