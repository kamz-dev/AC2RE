"""boot.py - start the game through the lab and get to gameplay on the last save.

  py tools/boot.py            launch, then title -> profile -> Continue -> Animus "load latest memory"
  py tools/boot.py --menu     only launch and pass the title (stop at the main menu)
  py tools/boot.py --close    close the running game (a normal window close)

The menus read the keyboard directly, so this focuses the game window and sends Enter (tools/keys.py). It assumes the
last used profile is selected and Continue is the default (the state of this install on 2026-10-12).
"""
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
LAB = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import keys  # noqa: E402
from lab import Lab  # noqa: E402


def running():
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq AssassinsCreed_Dx9.exe"], capture_output=True, text=True).stdout
    return "AssassinsCreed_Dx9.exe" in out


def close():
    if not running():
        return
    subprocess.run(["powershell", "-NoProfile", "-Command",
                    "$p = Get-Process AssassinsCreed_Dx9 -ErrorAction SilentlyContinue; if ($p) { $p.CloseMainWindow() | Out-Null; Start-Sleep 6 }"])


def status():
    try:
        return Lab(timeout=3).status()
    except OSError:
        return {}


def wait(pred, limit, what):
    t0 = time.time()
    while time.time() - t0 < limit:
        st = status()
        if pred(st):
            return st
        time.sleep(1)
    raise SystemExit(f"timed out waiting for {what}")


def enter(n=1, gap=3.0):
    for _ in range(n):
        keys.main(["enter"])
        time.sleep(gap)


def main(args):
    if "--close" in args:
        close()
        return 0
    if running():
        print("game already running")
    else:
        subprocess.Popen([os.path.join(LAB, "ac1lab.exe")], cwd=LAB, stdout=subprocess.DEVNULL)
    wait(lambda s: int(s.get("presents", "0")) > 300, 90, "the title screen")
    time.sleep(4)
    enter(2, 4)            # title -> profile list
    enter(1, 9)            # select the profile (loads, then waits for input)
    enter(1, 4)            # confirm
    if "--menu" in args:
        return 0
    enter(1, 12)           # main menu: Continue
    enter(1, 3)            # Animus: load latest memory
    st = wait(lambda s: s.get("human", "00000000") != "00000000", 180, "the player")
    print("in game:", st.get("ctx"), st.get("pos"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
