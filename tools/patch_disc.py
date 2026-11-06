#!/usr/bin/env python3
"""Puts a built executable into a copy of your PlayStation disc image.

    python3 tools/patch_disc.py game.cue build/us/SCUS_942.44 [-o game_nm.bin]

The image may be a .cue (its .bin is used), a raw .bin (2352-byte sectors)
or an .iso (2048-byte sectors). The boot executable named in SYSTEM.CNF is
replaced in place, so the new file must have the same size as the old one
(a NON_MATCHING build keeps every segment at its address, so it does). The
original image is not changed: the result is written next to it
(<name>_patched.bin) or to -o.

For raw images the EDC/ECC of the rewritten sectors is recomputed. The
script first checks its EDC/ECC code on untouched sectors of the same image;
if that check fails, it leaves EDC/ECC as they were (emulators usually do not
check them) and says so.
"""

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from disc_image import DiscError, DiscImage, load_image, RAW_SECTOR_SIZE, SECTOR_SIZE  # noqa: E402

# ---- EDC/ECC of CD-ROM sectors (as in ECMA-130) -----------------------------

ECC_F = [0] * 256
ECC_B = [0] * 256
EDC_LUT = [0] * 256
for _i in range(256):
    _j = ((_i << 1) ^ (0x11D if _i & 0x80 else 0)) & 0xFF
    ECC_F[_i] = _j
    ECC_B[_i ^ _j] = _i
    _e = _i
    for _ in range(8):
        _e = (_e >> 1) ^ (0xD8018001 if _e & 1 else 0)
    EDC_LUT[_i] = _e


def edc(data) -> int:
    e = 0
    for b in data:
        e = (e >> 8) ^ EDC_LUT[(e ^ b) & 0xFF]
    return e


def ecc_block(sector: bytearray, major_count, minor_count, major_mult, minor_inc, dest):
    src = 0xC
    size = major_count * minor_count
    for major in range(major_count):
        index = (major >> 1) * major_mult + (major & 1)
        a = b = 0
        for _ in range(minor_count):
            t = sector[src + index]
            index += minor_inc
            if index >= size:
                index -= size
            a ^= t
            b ^= t
            a = ECC_F[a]
        a = ECC_B[ECC_F[a] ^ b]
        sector[dest + major] = a
        sector[dest + major + major_count] = a ^ b


def fix_mode2_form1(sector: bytearray):
    """Recomputes EDC and ECC of a raw mode 2 form 1 sector."""
    e = edc(sector[0x10:0x818])
    sector[0x818:0x81C] = e.to_bytes(4, "little")
    header = sector[12:16]
    sector[12:16] = b"\0\0\0\0"  # mode 2: the ECC is computed with a zero header
    ecc_block(sector, 86, 24, 2, 86, 0x81C)
    ecc_block(sector, 52, 43, 86, 88, 0x8C8)
    sector[12:16] = header


def is_form1(sector) -> bool:
    return sector[15] == 2 and not (sector[18] & 0x20)


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.strip().splitlines()[0])
    parser.add_argument("image", type=Path, help=".cue, .bin or .iso of your disc")
    parser.add_argument("exe", type=Path, help="the built executable (build/us/SCUS_942.44)")
    parser.add_argument("-o", "--output", type=Path)
    args = parser.parse_args(argv)

    src = args.image
    if src.suffix.lower() == ".cue":
        import re
        m = re.search(r'^\s*FILE\s+"([^"]+)"', src.read_text(errors="replace"), re.MULTILINE)
        if not m:
            sys.exit(f"{src}: в .cue нет строки FILE")
        src = src.parent / m.group(1)
    data = bytearray(load_image(src))
    disc = DiscImage(bytes(data))
    name = disc.boot_executable()
    entry = disc.find(name)
    exe = args.exe.read_bytes()
    if len(exe) != entry["size"]:
        sys.exit(f"размер не совпадает: на диске {name} {entry['size']} байт, сборка {len(exe)} байт")

    sectors = (len(exe) + SECTOR_SIZE - 1) // SECTOR_SIZE
    if disc.raw:
        # check the EDC/ECC code on sectors of the image that stay as they are
        probe = [lba for lba in range(16, min(disc.sector_count, 400))
                 if not (entry["lba"] <= lba < entry["lba"] + sectors)]
        checked = ok = 0
        for lba in probe:
            s = bytearray(data[lba * RAW_SECTOR_SIZE:(lba + 1) * RAW_SECTOR_SIZE])
            if not is_form1(s):
                continue
            t = bytearray(s)
            fix_mode2_form1(t)
            checked += 1
            ok += t == s
            if checked >= 20:
                break
        fix = checked > 0 and ok == checked
        if not fix:
            print(f"проверка EDC/ECC не прошла ({ok}/{checked}): контрольные суммы не пересчитываются")
        for i in range(sectors):
            lba = entry["lba"] + i
            base = lba * RAW_SECTOR_SIZE
            mode = data[base + 15]
            off = base + (16 if mode == 1 else 24)
            chunk = exe[i * SECTOR_SIZE:(i + 1) * SECTOR_SIZE].ljust(SECTOR_SIZE, b"\0")
            if i == sectors - 1 and len(exe) % SECTOR_SIZE:
                # keep the rest of the last sector as it was
                n = len(exe) % SECTOR_SIZE
                chunk = exe[i * SECTOR_SIZE:] + bytes(data[off + n:off + SECTOR_SIZE])
            data[off:off + SECTOR_SIZE] = chunk
            if fix and mode == 2:
                s = bytearray(data[base:base + RAW_SECTOR_SIZE])
                fix_mode2_form1(s)
                data[base:base + RAW_SECTOR_SIZE] = s
    else:
        off = entry["lba"] * SECTOR_SIZE
        data[off:off + len(exe)] = exe

    out = args.output or src.with_name(src.stem + "_patched" + src.suffix)
    out.write_bytes(data)
    print(f"{name} заменён (LBA {entry['lba']}, {sectors} секторов): {out}")
    if src != args.image:
        cue = out.with_suffix(".cue")
        text = args.image.read_text(errors="replace").replace(src.name, out.name)
        cue.write_text(text)
        print(f"файл .cue для него: {cue}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except DiscError as e:
        sys.exit(f"ошибка: {e}")
