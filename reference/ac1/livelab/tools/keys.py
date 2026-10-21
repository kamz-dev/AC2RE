"""keys.py - focus the game window and send real keystrokes (scancodes, which DirectInput reads).

The title screen and the menus read the keyboard directly, so pad injection cannot drive them; gameplay can use the
lab's pad scripts instead.

  py tools/keys.py enter
  py tools/keys.py down down enter        (each key tapped ~0.1 s, 0.35 s apart)
  py tools/keys.py esc --back             (--back gives focus back to the previous window afterwards)
Keys: enter esc space up down left right tab e q f w a s d shift ctrl, or sc:<hex scancode>.
"""
import ctypes
import ctypes.wintypes as w
import sys
import time

user32 = ctypes.windll.user32

SC = {"esc": 0x01, "enter": 0x1C, "space": 0x39, "tab": 0x0F, "e": 0x12, "q": 0x10, "f": 0x21, "w": 0x11,
      "a": 0x1E, "s": 0x1F, "d": 0x20, "shift": 0x2A, "ctrl": 0x1D,
      "up": 0xC8, "down": 0xD0, "left": 0xCB, "right": 0xCD}

KEYEVENTF_KEYUP, KEYEVENTF_SCANCODE, KEYEVENTF_EXTENDEDKEY = 0x2, 0x8, 0x1


class KEYBDINPUT(ctypes.Structure):
    _fields_ = [("wVk", w.WORD), ("wScan", w.WORD), ("dwFlags", w.DWORD), ("time", w.DWORD),
                ("dwExtraInfo", ctypes.c_size_t)]


class INPUT(ctypes.Structure):
    class _U(ctypes.Union):
        _fields_ = [("ki", KEYBDINPUT), ("pad", ctypes.c_byte * 32)]
    _fields_ = [("type", w.DWORD), ("u", _U)]


def send(sc, up):
    flags = KEYEVENTF_SCANCODE | (KEYEVENTF_KEYUP if up else 0)
    if sc > 0x7F:
        flags |= KEYEVENTF_EXTENDEDKEY
        sc &= 0x7F
    i = INPUT()
    i.type = 1
    i.u.ki = KEYBDINPUT(0, sc, flags, 0, 0)
    user32.SendInput(1, ctypes.byref(i), ctypes.sizeof(INPUT))


def find_window():
    found = []

    @ctypes.WINFUNCTYPE(ctypes.c_bool, w.HWND, w.LPARAM)
    def cb(hwnd, _):
        n = user32.GetWindowTextLengthW(hwnd)
        buf = ctypes.create_unicode_buffer(n + 1)
        user32.GetWindowTextW(hwnd, buf, n + 1)
        if user32.IsWindowVisible(hwnd) and buf.value.startswith("Assassin"):
            found.append(hwnd)
        return True

    user32.EnumWindows(cb, 0)
    return found[0] if found else None


def focus(hwnd):
    # AttachThreadInput lets a background process take the foreground
    fg = user32.GetForegroundWindow()
    cur = user32.GetWindowThreadProcessId(fg, None)
    me = ctypes.windll.kernel32.GetCurrentThreadId()
    user32.AttachThreadInput(me, cur, True)
    user32.ShowWindow(hwnd, 9)
    user32.SetForegroundWindow(hwnd)
    user32.AttachThreadInput(me, cur, False)
    return fg


def main(args):
    back = "--back" in args
    keys = [a for a in args if not a.startswith("--")]
    hwnd = find_window()
    if not hwnd:
        print("game window not found")
        return 1
    prev = focus(hwnd)
    time.sleep(0.4)
    for k in keys:
        sc = int(k[3:], 16) if k.startswith("sc:") else SC[k.lower()]
        send(sc, False)
        time.sleep(0.1)
        send(sc, True)
        time.sleep(0.35)
    if back and prev:
        focus(prev)
    print("sent", keys)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
