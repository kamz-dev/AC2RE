"""lab.py - command line for the AC1 Lab bridge (127.0.0.1:27015).

  py tools/lab.py status
  py tools/lab.py "eval pos=$entity+0x40:vec3 ctx=[$ctx+0x10]+4:ctx"
  py tools/lab.py "play scripts/walk_forward.txt"
  py tools/lab.py -i                     (interactive)
  py tools/lab.py --wait-player 120      (wait until a player Human exists)

Commands (see README): status, frame, read <addr> <n>, eval <fields>, write <addr> <bytes>, writef <addr> <f>,
scale <f>, pause, resume, step [n], rec start <label>, rec stop, mark <text>, check <id|next|prev>, checks,
probe add <line>, probe clear, probes, play <script>, stop, inputrec on|off, save <slot>, load <slot>, overlay.
"""
import socket
import sys
import time

PORT = 27015


class Lab:
    def __init__(self, port=PORT, timeout=5.0):
        self.s = socket.create_connection(("127.0.0.1", port), timeout=timeout)
        self.buf = b""

    def cmd(self, line):
        self.s.sendall(line.encode() + b"\n")
        while b"\n.\n" not in self.buf and not self.buf.startswith(b".\n"):
            chunk = self.s.recv(65536)
            if not chunk:
                raise ConnectionError("bridge closed")
            self.buf += chunk
        if self.buf.startswith(b".\n"):
            reply, self.buf = b"", self.buf[2:]
        else:
            reply, self.buf = self.buf.split(b"\n.\n", 1)
        return reply.decode(errors="replace")

    def status(self):
        out = {}
        for part in self.cmd("status").split(" "):
            if "=" in part:
                k, v = part.split("=", 1)
                out[k] = v
        return out


def main(argv):
    if not argv:
        print(__doc__)
        return 0
    if argv[0] == "--wait-player":
        limit = float(argv[1]) if len(argv) > 1 else 120
        t0 = time.time()
        while time.time() - t0 < limit:
            try:
                st = Lab().status()
                if st.get("human", "00000000") != "00000000":
                    print("player:", st)
                    return 0
            except OSError:
                pass
            time.sleep(1)
        print("no player within", limit, "s")
        return 1
    lab = Lab()
    if argv[0] == "-i":
        while True:
            try:
                line = input("lab> ")
            except EOFError:
                return 0
            if line.strip() in ("quit", "exit"):
                return 0
            print(lab.cmd(line))
    for line in argv:
        print(lab.cmd(line))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
