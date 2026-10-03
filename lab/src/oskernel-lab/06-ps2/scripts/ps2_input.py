#!/usr/bin/env python3
"""ps2_input.py — test harness của Lab 0x06: chạy QEMU, gõ phím và di chuột THẬT qua QEMU monitor.

QEMU mở monitor ở một UNIX socket (-monitor unix:...). Script đợi kernel in
"[kernel] ready for input", rồi gửi lần lượt các lệnh monitor:
  sendkey <phím>              nhấn rồi nhả 1 phím (giữ 100 ms), vd "shift-h", "caps_lock", "ret"
  mouse_move <dx> <dy>        di chuột tương đối (dy > 0 = đi xuống màn hình)
  mouse_button <mask>         1 = trái, 2 = phải, 4 = giữa, 0 = nhả hết
Serial (COM1) của kernel đọc qua stdout của QEMU, ghi ra file.

Chạy (trong ~/oskernel-lab/06-ps2, sau `make image`):
    python3 scripts/ps2_input.py os.img run.txt
Thêm cờ QEMU sau "--":
    python3 scripts/ps2_input.py os.img run-kvm.txt -- -enable-kvm
"""
import os
import socket
import subprocess
import sys
import threading
import time

img = sys.argv[1] if len(sys.argv) > 1 else "os.img"
out_path = sys.argv[2] if len(sys.argv) > 2 else "run.txt"
extra = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
sock_path = f"/tmp/lab06-monitor-{os.getpid()}.sock"

QEMU = ["qemu-system-x86_64", "-M", "q35", "-m", "256M", "-serial", "stdio",
        "-display", "none", "-no-reboot", "-no-shutdown",
        "-monitor", f"unix:{sock_path},server,nowait",
        "-drive", f"file={img},format=raw,if=ide"] + extra

# Mỗi mục: (chú thích in ra log, [lệnh monitor...])
STEPS = [
    ("lowercase, left Shift, Space, Enter",
     ["sendkey shift-h", "sendkey i", "sendkey spc", "sendkey c", "sendkey a", "sendkey v",
      "sendkey shift-o", "sendkey shift-s", "sendkey ret"]),
    ("Caps Lock on: letters, a digit, Shift+letter; then Caps Lock off",
     ["sendkey caps_lock", "sendkey a", "sendkey b", "sendkey 1", "sendkey shift-c",
      "sendkey ret", "sendkey caps_lock"]),
    ("right Shift", ["sendkey shift_r-2", "sendkey shift_r-x", "sendkey ret"]),
    ("Backspace", ["sendkey a", "sendkey b", "sendkey backspace", "sendkey c", "sendkey ret"]),
    ("extended keys (E0 prefix) vs keypad 8", ["sendkey up", "sendkey down", "sendkey kp_8"]),
    ("F2 = translation off, 'a', F2 = translation on, 'a'",
     ["sendkey f2", "sendkey a", "sendkey f2", "sendkey a"]),
    ("mouse: right 10, up 5, left 3 + down 4, left 200",
     ["mouse_move 10 0", "mouse_move 0 -5", "mouse_move -3 4", "mouse_move -200 0"]),
    ("mouse buttons: left down/up, right down/up",
     ["mouse_button 1", "mouse_button 0", "mouse_button 2", "mouse_button 0"]),
    ("F7: the 0x41 read during initiateMouse left F7 marked as held in the evdev bitmap",
     ["sendkey f7"]),
    ("Esc ends the demo", ["sendkey esc"]),
]

lines = []
ready = threading.Event()
done = threading.Event()


def reader(pipe):
    for raw in iter(pipe.readline, b""):
        s = raw.decode("utf-8", "replace").rstrip("\r\n")
        lines.append(s)
        if "[kernel] ready for input" in s:
            ready.set()
        if "[kernel] done" in s:
            done.set()


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
    if not ready.wait(60):
        raise SystemExit("kernel never printed 'ready for input'")
    mon = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    mon.connect(sock_path)
    buf = b""
    while not buf.endswith(b"(qemu) "):           # banner + dấu nhắc đầu tiên
        buf += mon.recv(4096)
    for title, cmds in STEPS:
        lines.append(f"##### harness: {title}")
        time.sleep(0.3)
        for c in cmds:
            lines.append(f"##### harness: {c}")
            monitor_cmd(mon, c)
            time.sleep(0.35)                         # sendkey giữ phím 100 ms; chờ kernel in xong
    done.wait(10)
    time.sleep(0.5)
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
