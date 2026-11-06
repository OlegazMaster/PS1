"""Runs GTE (COP2) instructions in unicorn with the software GTE of the port.

unicorn has no GTE. GteHook finds the COP2 instructions (mfc2, cfc2, mtc2,
ctc2, lwc2, swc2 and the commands) in the given code ranges, replaces them
with nop and performs each one in a hook that runs just before it, through
src/pc/gte.c built as a host library. This works in branch delay slots too.
The results become visible at once (the load delay of mfc2/cfc2 is not
modelled).
"""
import ctypes
import os
import struct
import subprocess

from unicorn import UC_HOOK_CODE, UcError, UC_ERR_READ_UNALIGNED, UC_ERR_WRITE_UNALIGNED
from unicorn.mips_const import UC_MIPS_REG_0

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIB = os.path.join(ROOT, "build", "us", "libgte_soft.so")


class GteState(ctypes.Structure):
    _fields_ = [("data", ctypes.c_uint32 * 32), ("ctrl", ctypes.c_uint32 * 32)]


def load_lib():
    src = os.path.join(ROOT, "src", "pc", "gte.c")
    if not os.path.exists(LIB) or os.path.getmtime(LIB) < os.path.getmtime(src):
        os.makedirs(os.path.dirname(LIB), exist_ok=True)
        subprocess.run(["cc", "-O1", "-shared", "-fPIC", "-I" + os.path.join(ROOT, "include"), "-o", LIB, src],
                       check=True)
    lib = ctypes.CDLL(LIB)
    lib.gte_read_data.restype = ctypes.c_uint32
    lib.gte_read_ctrl.restype = ctypes.c_uint32
    lib.gte_write_data.argtypes = [ctypes.c_int, ctypes.c_uint32]
    lib.gte_write_ctrl.argtypes = [ctypes.c_int, ctypes.c_uint32]
    lib.gte_command.argtypes = [ctypes.c_uint32]
    return lib


def is_cop2(insn):
    op = insn >> 26
    if op in (0x32, 0x3A):  # lwc2, swc2
        return True
    if op != 0x12:
        return False
    return bool(insn & 0x02000000) or ((insn >> 21) & 0x1F) in (0, 2, 4, 6)


class GteHook:
    def __init__(self, mu, ranges):
        """ranges: (start, end) virtual addresses of code to scan."""
        self.mu = mu
        self.lib = load_lib()
        self.state = GteState.in_dll(self.lib, "gte")
        self.insns = {}
        self.on_fault = None  # f(access, address): a lwc2/swc2 on unmapped memory
        for start, end in ranges:
            code = bytes(mu.mem_read(start & 0x1FFFFFFF, end - start))
            for off in range(0, len(code) - 3, 4):
                insn = struct.unpack_from("<I", code, off)[0]
                if is_cop2(insn):
                    self.insns[start + off] = insn
        for addr in self.insns:
            mu.mem_write(addr & 0x1FFFFFFF, b"\0\0\0\0")
            mu.hook_add(UC_HOOK_CODE, self._hook, begin=addr, end=addr)

    def reset(self, data=None, ctrl=None):
        for i in range(32):
            self.state.data[i] = (data or [0] * 32)[i]
            self.state.ctrl[i] = (ctrl or [0] * 32)[i]

    def _reg(self, n):
        return self.mu.reg_read(UC_MIPS_REG_0 + n)

    def _set(self, n, v):
        if n:
            self.mu.reg_write(UC_MIPS_REG_0 + n, v & 0xFFFFFFFF)

    def _hook(self, uc, addr, size, user):
        insn = self.insns[addr]
        op = insn >> 26
        rt = (insn >> 16) & 31
        if op in (0x32, 0x3A):
            base = (insn >> 21) & 31
            ea = (self._reg(base) + struct.unpack("<h", struct.pack("<H", insn & 0xFFFF))[0]) & 0xFFFFFFFF
            phys = ea & 0x1FFFFFFF if ea >= 0x80000000 else ea
            if ea & 3:  # an address error, as lw/sw would raise
                raise UcError(UC_ERR_READ_UNALIGNED if op == 0x32 else UC_ERR_WRITE_UNALIGNED)
            try:
                if op == 0x32:
                    self.lib.gte_write_data(rt, struct.unpack("<I", uc.mem_read(phys, 4))[0])
                else:
                    uc.mem_write(phys, struct.pack("<I", self.lib.gte_read_data(rt)))
            except UcError:
                if self.on_fault and ea < 0xC0000000:
                    # as UC_HOOK_MEM_INVALID would report it: nothing for
                    # kseg2, the physical address for kseg0/kseg1
                    self.on_fault(19 if op == 0x32 else 20, ea & 0x1FFFFFFF if ea >= 0x80000000 else ea)
                raise
            return
        if insn & 0x02000000:
            self.lib.gte_command(insn & 0x1FFFFFF)
            return
        rs = (insn >> 21) & 31
        rd = (insn >> 11) & 31
        if rs == 0:
            self._set(rt, self.lib.gte_read_data(rd))
        elif rs == 2:
            self._set(rt, self.lib.gte_read_ctrl(rd))
        elif rs == 4:
            self.lib.gte_write_data(rd, self._reg(rt))
        else:
            self.lib.gte_write_ctrl(rd, self._reg(rt))

    def hook_call(self, addr):
        """Performs the calls of a function u32 f(u32 op, u32 v) at addr
        (rc_gte_op of include/nd_recomp.h) on the software GTE: op 0x000 +
        reg mtc2, 0x100 + reg mfc2, 0x200 + reg ctc2, 0x300 + reg cfc2, 0x400
        a command. The function itself only returns (jr $ra; nop)."""
        self.mu.mem_write(addr & 0x1FFFFFFF, struct.pack("<2I", 0x03E00008, 0))
        self.mu.hook_add(UC_HOOK_CODE, self._call, begin=addr, end=addr)

    def _call(self, uc, addr, size, user):
        op, v = self._reg(4), self._reg(5)
        kind, reg = op >> 8, op & 31
        if kind == 0:
            self.lib.gte_write_data(reg, v)
        elif kind == 1:
            self._set(2, self.lib.gte_read_data(reg))
        elif kind == 2:
            self.lib.gte_write_ctrl(reg, v)
        elif kind == 3:
            self._set(2, self.lib.gte_read_ctrl(reg))
        else:
            self.lib.gte_command(v & 0x1FFFFFF)
