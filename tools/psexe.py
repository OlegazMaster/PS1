#!/usr/bin/env python3
"""Reads and checks PlayStation executables (PS-X EXE).

Header layout: https://psx-spx.consoledev.net/cdromdrive/#filenameexe-general-purpose-executable

Usage: psexe.py <path-to-exe>
"""

import hashlib
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

MAGIC = b"PS-X EXE"
HEADER_SIZE = 0x800
SECTOR_SIZE = 0x800

REGION_MARKERS = {
    "North America": "NTSC-U",
    "Europe": "PAL",
    "Japan": "NTSC-J",
}

# MIPS opcodes used to recover the initial $gp from the entry point code.
OP_ADDIU = 0x09
OP_ORI = 0x0D
OP_LUI = 0x0F
REG_GP = 28


@dataclass
class PsxExe:
    entry: int
    gp: int
    load_addr: int
    text_size: int
    data_addr: int
    data_size: int
    bss_addr: int
    bss_size: int
    stack_base: int
    stack_offset: int
    marker: str
    file_size: int
    sha1: str

    @property
    def region(self) -> str:
        for needle, region in REGION_MARKERS.items():
            if needle in self.marker:
                return region
        return "unknown"

    @property
    def end_addr(self) -> int:
        return self.load_addr + self.text_size

    def vram_to_offset(self, vram: int) -> int:
        return vram - self.load_addr + HEADER_SIZE

    def offset_to_vram(self, offset: int) -> int:
        return offset - HEADER_SIZE + self.load_addr


class PsxExeError(Exception):
    pass


def parse(data: bytes) -> PsxExe:
    if len(data) < HEADER_SIZE or data[:8] != MAGIC:
        raise PsxExeError("это не исполняемый файл PlayStation (PS-X EXE)")

    (entry, gp, load_addr, text_size, data_addr, data_size, bss_addr, bss_size,
     stack_base, stack_offset) = struct.unpack_from("<10I", data, 0x10)
    marker = data[0x4C:HEADER_SIZE].split(b"\0", 1)[0].decode("ascii", "replace")

    return PsxExe(
        entry=entry,
        gp=gp,
        load_addr=load_addr,
        text_size=text_size,
        data_addr=data_addr,
        data_size=data_size,
        bss_addr=bss_addr,
        bss_size=bss_size,
        stack_base=stack_base,
        stack_offset=stack_offset,
        marker=marker,
        file_size=len(data),
        sha1=hashlib.sha1(data).hexdigest(),
    )


def validate(exe: PsxExe) -> list:
    """Returns a list of problems; an empty list means the header looks sane."""
    problems = []
    if exe.text_size % SECTOR_SIZE:
        problems.append(f"размер кода 0x{exe.text_size:X} не кратен 0x800")
    if exe.file_size != HEADER_SIZE + exe.text_size:
        problems.append(
            f"размер файла 0x{exe.file_size:X} не совпадает с заголовком "
            f"(0x800 + 0x{exe.text_size:X})"
        )
    if not 0x80000000 <= exe.load_addr < 0x80200000:
        problems.append(f"адрес загрузки 0x{exe.load_addr:08X} вне основной памяти")
    if not exe.load_addr <= exe.entry < exe.end_addr:
        problems.append(f"точка входа 0x{exe.entry:08X} вне загружаемого кода")
    return problems


def find_gp(data: bytes, exe: PsxExe, max_instructions: int = 64) -> int:
    """Returns $gp as set by the header or by the startup code, 0 if unused."""
    if exe.gp:
        return exe.gp

    gp = 0
    start = exe.vram_to_offset(exe.entry)
    end = min(start + max_instructions * 4, len(data))
    for offset in range(start, end - 3, 4):
        word = struct.unpack_from("<I", data, offset)[0]
        opcode = word >> 26
        rs = (word >> 21) & 0x1F
        rt = (word >> 16) & 0x1F
        imm = word & 0xFFFF
        if rt != REG_GP:
            continue
        if opcode == OP_LUI:
            gp = imm << 16
        elif opcode == OP_ADDIU and rs == REG_GP:
            gp = (gp + (imm - 0x10000 if imm & 0x8000 else imm)) & 0xFFFFFFFF
        elif opcode == OP_ORI and rs == REG_GP:
            gp |= imm
    return gp


def load(path: Path) -> tuple:
    data = Path(path).read_bytes()
    return data, parse(data)


def main(argv):
    if len(argv) != 2:
        raise SystemExit(__doc__.strip().splitlines()[-1])

    try:
        data, exe = load(Path(argv[1]))
    except (OSError, PsxExeError) as e:
        raise SystemExit(f"{argv[1]}: {e}")

    gp = find_gp(data, exe)
    print(f"файл:          {argv[1]}")
    print(f"sha1:          {exe.sha1}")
    print(f"размер:        0x{exe.file_size:X}")
    print(f"регион:        {exe.region} ({exe.marker})")
    print(f"точка входа:   0x{exe.entry:08X}")
    print(f"адреса кода:   0x{exe.load_addr:08X}-0x{exe.end_addr:08X}")
    print(f"$gp:           " + (f"0x{gp:08X}" if gp else "не используется"))
    if exe.bss_size:
        print(f"bss:           0x{exe.bss_addr:08X} (+0x{exe.bss_size:X})")
    print(f"стек:          0x{exe.stack_base + exe.stack_offset:08X}")

    problems = validate(exe)
    for problem in problems:
        print(f"предупреждение: {problem}")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
