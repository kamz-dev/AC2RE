"""Read-only memory reader for the running AC2 (ReadProcessMemory; never writes, never injects).

A stopgap until the lab mod exists. Addresses are PC IDB addresses (RVAs); add `Proc().base`.
`player_human_ground(p)` follows the chain in RE/20_live_checks.md (session 1).
"""
import ctypes
import ctypes.wintypes as W
import struct

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
psapi = ctypes.WinDLL("psapi", use_last_error=True)
PROCESS_QUERY_INFORMATION, PROCESS_VM_READ = 0x400, 0x10


class MBI(ctypes.Structure):
    _fields_ = [("BaseAddress", ctypes.c_uint64), ("AllocationBase", ctypes.c_uint64), ("AllocationProtect", W.DWORD),
                ("__a", W.DWORD), ("RegionSize", ctypes.c_uint64), ("State", W.DWORD), ("Protect", W.DWORD),
                ("Type", W.DWORD), ("__b", W.DWORD)]


def find_pid(name="AssassinsCreedIIGame.exe"):
    arr = (W.DWORD * 4096)()
    n = W.DWORD()
    psapi.EnumProcesses(arr, ctypes.sizeof(arr), ctypes.byref(n))
    for pid in arr[: n.value // 4]:
        h = k32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, pid)
        if not h:
            continue
        buf = ctypes.create_unicode_buffer(260)
        psapi.GetModuleBaseNameW(h, None, buf, 260)
        k32.CloseHandle(h)
        if buf.value.lower() == name.lower():
            return pid
    return None


class Proc:
    def __init__(self):
        self.pid = find_pid()
        assert self.pid, "game not running"
        self.h = k32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, self.pid)
        mods = (ctypes.c_void_p * 1024)()
        n = W.DWORD()
        psapi.EnumProcessModulesEx(self.h, mods, ctypes.sizeof(mods), ctypes.byref(n), 0x01)  # 32-bit modules
        self.base = None
        for m in mods[: n.value // ctypes.sizeof(ctypes.c_void_p)]:
            buf = ctypes.create_unicode_buffer(260)
            psapi.GetModuleBaseNameW(self.h, ctypes.c_void_p(m), buf, 260)
            if buf.value.lower() == "assassinscreediigame.exe":
                self.base = m
        assert self.base is not None

    def read(self, addr, n):
        buf = ctypes.create_string_buffer(n)
        got = ctypes.c_size_t()
        if not k32.ReadProcessMemory(self.h, ctypes.c_void_p(addr), buf, n, ctypes.byref(got)):
            return None
        return buf.raw[: got.value]

    def u32(self, a):
        b = self.read(a, 4)
        return struct.unpack("<I", b)[0] if b else None

    def f32(self, a):
        b = self.read(a, 4)
        return struct.unpack("<f", b)[0] if b else None

    def regions(self):
        a = 0
        mbi = MBI()
        while a < 0xFFFF0000:
            if not k32.VirtualQueryEx(self.h, ctypes.c_void_p(a), ctypes.byref(mbi), ctypes.sizeof(mbi)):
                break
            if mbi.State == 0x1000 and mbi.Protect in (0x04, 0x40) and mbi.Type == 0x20000:  # committed private RW
                yield mbi.BaseAddress, mbi.RegionSize
            a = mbi.BaseAddress + mbi.RegionSize

    def scan_u32(self, value):
        pat = struct.pack("<I", value)
        out = []
        for b, s in self.regions():
            data = self.read(b, s)
            if not data:
                continue
            i = data.find(pat)
            while i >= 0:
                if i % 4 == 0:
                    out.append(b + i)
                i = data.find(pat, i + 1)
        return out


def player_entity(p):
    """World::GetPlayer (0x3A300) on EngineContextImp::ms_Instance (0x1E10AC0) +0x4C."""
    world = p.u32(p.u32(p.base + 0x1E10AC0) + 0x4C)
    return p.u32(p.u32(p.u32(world + 0x53C) + 4) + 4 * p.u32(p.u32(p.base + 0x1E3DBC0) + 0x24))


def player_human_ground(p):
    """(entity, Human, HumanGround) of the player. The HumanGround is the one whose +0x08 is the Human."""
    ent = player_entity(p)
    human = p.u32(p.u32(p.u32(ent + 0x74) + 0x54) + 0x2A4)
    hg = p.u32(human + 0x440)  # the context bundle's first slot (RE/20 row 9)
    if p.u32(hg) != p.base + 0x19BFCD8 or p.u32(hg + 8) != human:
        hg = next(o for o in p.scan_u32(p.base + 0x19BFCD8) if p.u32(o + 8) == human)
    return ent, human, hg
