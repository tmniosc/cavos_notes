#!/usr/bin/env python3
"""tasks_input.py — test harness của Lab 0x07 (chép từ ps2_input.py của Lab 0x06).

Chạy QEMU với -monitor unix:..., đọc serial (stdout của QEMU), đợi một dòng chữ rồi gõ
phím THẬT bằng lệnh monitor "sendkey". Thread "kbd" của kernel đang chờ ở WAITING_INPUT;
IRQ1 đánh thức nó.

Chạy (trong ~/oskernel-lab/07-tasks, sau `make image`):
    python3 scripts/tasks_input.py os.img run.txt                 # demo chính
    python3 scripts/tasks_input.py os.img run.txt --lost-wakeup   # build DEMO=-DLOST_WAKEUP
Thêm cờ QEMU sau "--":
    python3 scripts/tasks_input.py os.img run.txt -- -enable-kvm
"""
import os
import socket
import subprocess
import sys
import threading
import time

args = sys.argv[1:]
extra = args[args.index("--") + 1:] if "--" in args else []
if "--" in args:
    args = args[:args.index("--")]
lost = "--lost-wakeup" in args
args = [a for a in args if a != "--lost-wakeup"]
img = args[0] if len(args) > 0 else "os.img"
out_path = args[1] if len(args) > 1 else "run.txt"
sock_path = f"/tmp/lab07-monitor-{os.getpid()}.sock"

QEMU = ["qemu-system-x86_64", "-M", "q35", "-m", "256M", "-serial", "stdio",
        "-display", "none", "-no-reboot", "-no-shutdown",
        "-monitor", f"unix:{sock_path},server,nowait",
        "-drive", f"file={img},format=raw,if=ide"] + extra


def word(w):
    return [("key", "spc" if ch == " " else ch) for ch in w] + [("key", "ret")]


# ("wait", chữ cần thấy trong serial) | ("key", tên phím sendkey) | ("sleep", giây)
if lost:
    STEPS = [("wait", "LOST_WAKEUP: ring empty"), ("sleep", 0.1),
             ("note", "key 'h' typed inside the 300 ms gap"), ("key", "h"),
             ("sleep", 1.0),
             ("note", "key 'i' typed 1 s later"), ("key", "i"),
             ("sleep", 0.5)] + word("") + [("sleep", 0.5)] + word("quit")
else:
    STEPS = [("wait", "ready for input"), ("sleep", 0.05)] + word("hello") + \
            [("sleep", 0.6)] + word("quit")

lines = []
seen = threading.Condition()
done = threading.Event()


def reader(pipe):
    for raw in iter(pipe.readline, b""):
        s = raw.decode("utf-8", "replace").rstrip("\r\n")
        with seen:
            lines.append(s)
            seen.notify_all()
        if "[kernel] done" in s:
            done.set()


def wait_text(text, timeout=60):
    end = time.time() + timeout
    with seen:
        while not any(text in l for l in lines):
            left = end - time.time()
            if left <= 0:
                raise SystemExit(f"never saw {text!r}")
            seen.wait(left)


def monitor_cmd(sock, cmd):
    sock.sendall((cmd + "\n").encode())
    buf = b""
    while not buf.endswith(b"(qemu) "):
        chunk = sock.recv(4096)
        if not chunk:
            break
        buf += chunk
    return buf


proc = subprocess.Popen(QEMU, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
threading.Thread(target=reader, args=(proc.stdout,), daemon=True).start()

try:
    time.sleep(0.5)
    mon = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    for _ in range(50):
        try:
            mon.connect(sock_path)
            break
        except OSError:
            time.sleep(0.1)
    buf = b""
    while not buf.endswith(b"(qemu) "):           # banner + dấu nhắc đầu tiên
        buf += mon.recv(4096)
    for kind, val in STEPS:
        if kind == "wait":
            wait_text(val)
        elif kind == "sleep":
            time.sleep(val)
        elif kind == "note":
            with seen:
                lines.append(f"##### harness: {val}")
        else:
            with seen:
                lines.append(f"##### harness: sendkey {val}")
            monitor_cmd(mon, f"sendkey {val}")
            time.sleep(0.15)                       # sendkey giữ phím 100 ms
    done.wait(30)
    time.sleep(0.3)
    monitor_cmd(mon, "quit")
finally:
    try:
        proc.wait(5)
    except subprocess.TimeoutExpired:
        proc.kill()
    try:
        os.unlink(sock_path)
    except OSError:
        pass

with open(out_path, "w") as f:
    f.write("\n".join(lines) + "\n")
print(f"wrote {len(lines)} lines to {out_path}; kernel finished: {done.is_set()}")
