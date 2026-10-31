"""Tests for the helper scripts in tools/ (run with `make test`)."""

import io
import struct
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import disc_image  # noqa: E402
import gen_config  # noqa: E402
import progress  # noqa: E402
import comm_globl  # noqa: E402
import psexe  # noqa: E402
import versions  # noqa: E402

LOAD_ADDR = 0x80010000

# lui $gp, 0x8007 / addiu $gp, $gp, -0x7FF0 / jr $ra / nop
STARTUP_CODE = [0x3C1C8007, 0x279C8010, 0x03E00008, 0x00000000]


def make_exe(code=STARTUP_CODE, entry=LOAD_ADDR, gp=0,
             marker=b"Sony Computer Entertainment Inc. for North America area"):
    payload = struct.pack(f"<{len(code)}I", *code).ljust(psexe.SECTOR_SIZE, b"\0")
    header = bytearray(psexe.HEADER_SIZE)
    header[0:8] = psexe.MAGIC
    struct.pack_into("<10I", header, 0x10, entry, gp, LOAD_ADDR, len(payload),
                     0, 0, 0, 0, 0x801FFFF0, 0)
    header[0x4C:0x4C + len(marker)] = marker
    return bytes(header) + payload


class PsxExeTest(unittest.TestCase):
    def test_parse(self):
        exe = psexe.parse(make_exe())
        self.assertEqual(exe.entry, LOAD_ADDR)
        self.assertEqual(exe.load_addr, LOAD_ADDR)
        self.assertEqual(exe.text_size, 0x800)
        self.assertEqual(exe.region, "NTSC-U")
        self.assertEqual(psexe.validate(exe), [])

    def test_rejects_other_files(self):
        with self.assertRaises(psexe.PsxExeError):
            psexe.parse(b"\0" * 0x1000)

    def test_validate_reports_bad_size(self):
        exe = psexe.parse(make_exe() + b"\0" * 4)
        self.assertTrue(any("размер файла" in p for p in psexe.validate(exe)))

    def test_gp_from_startup_code(self):
        data = make_exe()
        self.assertEqual(psexe.find_gp(data, psexe.parse(data)), 0x80068010)

    def test_gp_from_header(self):
        data = make_exe(gp=0x80091234)
        self.assertEqual(psexe.find_gp(data, psexe.parse(data)), 0x80091234)

    def test_no_gp(self):
        data = make_exe(code=[0x03E00008, 0])
        self.assertEqual(psexe.find_gp(data, psexe.parse(data)), 0)

    def test_regions(self):
        for marker, region in [(b"... for Europe area", "PAL"), (b"... for Japan area", "NTSC-J")]:
            self.assertEqual(psexe.parse(make_exe(marker=marker)).region, region)


class GenConfigTest(unittest.TestCase):
    def render(self, text_start=0x900, data_start=0xA00, gp=0x80068010):
        exe = psexe.parse(make_exe())
        return gen_config.render_yaml("us", versions.get("us"), exe, gp, text_start, data_start)

    def test_segments(self):
        text = self.render()
        self.assertIn('- [0x800, rodata, "800"]', text)
        self.assertIn('- [0x900, asm, "900"]', text)
        self.assertIn('- [0xA00, data, "A00"]', text)
        self.assertIn("- [0x1000]", text)
        self.assertIn("gp_value: 0x80068010", text)
        self.assertIn("vram: 0x80010000", text)

    def test_code_at_start_has_no_rodata(self):
        text = self.render(text_start=0x800, data_start=0, gp=0)
        self.assertNotIn("rodata", text.split("segments:")[1])
        self.assertNotIn(", data,", text)
        self.assertIn("# gp_value", text)

    def test_valid_yaml(self):
        try:
            import yaml
        except ImportError:
            self.skipTest("PyYAML is not installed")
        config = yaml.safe_load(self.render())
        self.assertEqual(config["options"]["platform"], "psx")
        self.assertEqual(config["options"]["target_path"], "disc/SCUS_942.44")
        self.assertEqual(config["segments"][1]["subsegments"][1], [0x900, "asm", "900"])


ASM_FUNCTION = """\
glabel {name}
    /* 800 80010000 E8FFBD27 */  addiu      $sp, $sp, -0x18
    /* 804 80010004 1000BFAF */  sw         $ra, 0x10($sp)
    /* 808 80010008 0800E003 */  jr         $ra
    /* 80C 8001000C 00000000 */   nop
endlabel {name}
"""


class ProgressTest(unittest.TestCase):
    def test_counts_linked_functions(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            asm = root / "asm" / "us"
            funcs = asm / "nonmatchings" / "engine" / "main"
            funcs.mkdir(parents=True)
            (funcs / "func_todo.s").write_text(ASM_FUNCTION.format(name="func_todo"))
            matched = asm / "matchings" / "engine" / "main"
            matched.mkdir(parents=True)
            (matched / "func_done.s").write_text(ASM_FUNCTION.format(name="func_done"))
            (asm / "900.s").write_text(
                '.section .text, "ax"\n' + ASM_FUNCTION.format(name="func_segment")
                + '.section .rodata\nglabel some_data\n    /* 900 80010100 */ .word 0x1\n'
            )
            (asm / "data").mkdir()
            (asm / "data" / "800.rodata.s").write_text(
                '.section .rodata\ndlabel D_80010000\n    /* 800 80010000 */ .word 0x1\n')
            # Left over from an earlier split and no longer linked.
            (asm / "800.s").write_text(ASM_FUNCTION.format(name="func_done"))
            (asm / "main.ld").write_text(
                "    build/us/asm/us/data/800.rodata.s.o(.rodata);\n"
                "    build/us/src/engine/main.c.o(.rodata);\n"
                "    build/us/src/engine/main.c.o(.text);\n"
                "    build/us/asm/us/900.s.o(.text);\n"
                "    build/us/asm/us/psyq.s.o(.text);\n"
            )
            (asm / "psyq.s").write_text(ASM_FUNCTION.format(name="CdInit"))
            src = root / "src" / "engine"
            src.mkdir(parents=True)
            (src / "main.c").write_text(
                '#include "common.h"\n\n'
                'void func_done(void) {\n}\n\n'
                'INCLUDE_ASM("asm/us/nonmatchings/engine/main", func_todo);\n'
            )

            all_stats = progress.scan(root, "us")
            stats = all_stats[progress.GAME]
            self.assertEqual(stats.total_funcs, 3)
            self.assertEqual(stats.done_funcs, 1)
            self.assertEqual(stats.total_bytes, 48)
            self.assertEqual(stats.done_bytes, 16)
            self.assertEqual(set(stats.remaining), {"func_todo", "func_segment"})
            self.assertEqual(set(all_stats[progress.CATEGORIES["psyq"]].remaining), {"CdInit"})

    def test_function_written_after_extract(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            asm = root / "asm" / "us"
            funcs = asm / "nonmatchings" / "sys"
            funcs.mkdir(parents=True)
            for name in ("func_new", "func_parked", "func_old"):
                (funcs / f"{name}.s").write_text(ASM_FUNCTION.format(name=name))
            (asm / "main.ld").write_text("    build/us/src/sys.c.o(.text);\n")
            (root / "src").mkdir()
            (root / "src" / "sys.c").write_text(
                '#include "common.h"\n\n'
                's32 func_new(DrawContext *dc) {\n    return 0;\n}\n\n'
                'void func_new(void);\n\n'
                '#ifdef NON_MATCHING\nvoid func_parked(void) {\n}\n#else\n'
                'INCLUDE_ASM("asm/us/nonmatchings/sys", func_parked);\n#endif\n'
            )

            stats = progress.scan(root, "us")[progress.GAME]
            # func_old was renamed away: neither C nor INCLUDE_ASM uses it.
            self.assertEqual(stats.total_funcs, 2)
            self.assertEqual(stats.done_funcs, 1)
            self.assertEqual(set(stats.remaining), {"func_parked"})


def dir_record(name: bytes, lba: int, size: int, is_dir: bool = False) -> bytes:
    length = 33 + len(name) + (len(name) + 1) % 2
    record = bytearray(length)
    record[0] = length
    struct.pack_into("<I", record, 2, lba)
    struct.pack_into(">I", record, 6, lba)
    struct.pack_into("<I", record, 10, size)
    struct.pack_into(">I", record, 14, size)
    record[25] = 2 if is_dir else 0
    record[32] = len(name)
    record[33:33 + len(name)] = name
    return bytes(record)


def make_iso(files: dict) -> list:
    """Builds the 2048-byte sectors of a tiny ISO 9660 image: root + one subdirectory."""
    sectors = [bytes(2048)] * 16
    root_lba, sub_lba, first_file_lba = 18, 19, 20
    lba = first_file_lba
    root, sub, contents = [], [], []
    for path, data in files.items():
        entries = sub if "/" in path else root
        entries.append(dir_record(path.split("/")[-1].encode() + b";1", lba, len(data)))
        contents.append(data)
        lba += (len(data) + 2047) // 2048
    root = [dir_record(b"\0", root_lba, 2048, True), dir_record(b"\1", root_lba, 2048, True),
            dir_record(b"S0", sub_lba, 2048, True)] + root
    sub = [dir_record(b"\0", sub_lba, 2048, True), dir_record(b"\1", root_lba, 2048, True)] + sub

    pvd = bytearray(2048)
    pvd[0] = 1
    pvd[1:6] = b"CD001"
    pvd[156:190] = dir_record(b"\0", root_lba, 2048, True)
    sectors += [bytes(pvd), bytes(2048), b"".join(root).ljust(2048, b"\0"), b"".join(sub).ljust(2048, b"\0")]
    for data in contents:
        for i in range(0, max(len(data), 1), 2048):
            sectors.append(data[i:i + 2048].ljust(2048, b"\0"))
    return sectors


def to_raw_mode2(sectors: list) -> bytes:
    raw = b""
    for user_data in sectors:
        header = disc_image.SYNC + bytes([0, 2, 0, 2]) + bytes(8)
        raw += (header + user_data).ljust(disc_image.RAW_SECTOR_SIZE, b"\0")
    return raw


class DiscImageTest(unittest.TestCase):
    FILES = {
        "SYSTEM.CNF": b"BOOT = cdrom:\\SCUS_942.44;1\r\nTCB = 4\r\n",
        "SCUS_942.44": bytes(range(256)) * 20,
        "S0/S0000002.NSF": b"level data",
    }

    def check(self, image: bytes):
        disc = disc_image.DiscImage(image)
        self.assertEqual(disc.boot_executable(), "SCUS_942.44")
        self.assertEqual(disc.read_file("SCUS_942.44"), self.FILES["SCUS_942.44"])
        self.assertEqual(disc.read_file("s0/s0000002.nsf"), b"level data")
        self.assertEqual({path for path, _ in disc.walk()},
                         {"S0", "S0/S0000002.NSF", "SYSTEM.CNF", "SCUS_942.44"})
        with self.assertRaises(disc_image.DiscError):
            disc.read_file("MISSING.BIN")

    def test_iso(self):
        self.check(b"".join(make_iso(self.FILES)))

    def test_raw_mode2(self):
        self.check(to_raw_mode2(make_iso(self.FILES)))

    def test_partial_image(self):
        sectors = make_iso(self.FILES)
        # Cut the image inside the last file: everything before it still reads.
        disc = disc_image.DiscImage(to_raw_mode2(sectors[:-1]))
        self.assertEqual(disc.read_file("SCUS_942.44"), self.FILES["SCUS_942.44"])
        with self.assertRaises(disc_image.DiscError):
            disc.read_file("S0/S0000002.NSF")


    def test_store_7z_volume(self):
        with tempfile.TemporaryDirectory() as tmp:
            volume = Path(tmp) / "game.7z.001"
            header = disc_image.SEVENZIP_MAGIC.ljust(disc_image.SEVENZIP_HEADER_SIZE, b"\0")
            volume.write_bytes(header + to_raw_mode2(make_iso(self.FILES)))
            self.check(disc_image.load_image(volume))

            volume.write_bytes(header + bytes(range(256)) * 200)
            with self.assertRaises(disc_image.DiscError):
                disc_image.load_image(volume)


class CommGloblTest(unittest.TestCase):
    def test_declares_comm_symbols_global(self):
        src = "\t.comm\tpause_stamp,4\n\t.lcomm\tlocal_var,4\n\tlw\t$2,x\n"
        out = io.StringIO()
        with mock.patch("sys.stdin", io.StringIO(src)), mock.patch("sys.stdout", out):
            comm_globl.main()
        self.assertEqual(out.getvalue(), "\t.globl\tpause_stamp\n" + src)


class VersionsTest(unittest.TestCase):
    def test_known_versions(self):
        self.assertEqual(versions.get("us").exe, "SCUS_942.44")
        with self.assertRaises(SystemExit):
            versions.get("xx")


if __name__ == "__main__":
    unittest.main()
