#!/usr/bin/env python3
"""Launch a C64 .prg in VICE, monitor $C000, quit when it stops changing.

Usage: python vice_run.py <program.prg> [--printer <output_file>] [--timeout <seconds>]
"""

import sys
import os
import socket
import subprocess
import time

VICE_PATH = r"C:\games\GTK3VICE-3.9-win64\bin\x64sc.exe"
MONITOR_PORT = 6510
WATCH_ADDR = 0xC000
DEFAULT_TIMEOUT = 10


def die(msg, proc):
    print(f"[vice_run] ERROR: {msg}")
    if proc.poll() is None:
        print("[vice_run] Killing VICE...")
        proc.kill()
    proc.wait()
    print("[vice_run] VICE is dead.")
    sys.exit(1)


def connect_monitor(proc, retries=30, delay=0.3):
    for i in range(retries):
        if proc.poll() is not None:
            return None
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(2.0)
            sock.connect(("127.0.0.1", MONITOR_PORT))
            try:
                sock.recv(4096)
            except socket.timeout:
                pass
            print(f"[vice_run] Connected to monitor (attempt {i+1})")
            return sock
        except (ConnectionRefusedError, socket.timeout, OSError):
            sock.close()
            time.sleep(delay)
    return None


def send_cmd(sock, cmd):
    """Send command, read response."""
    sock.sendall((cmd + "\n").encode("ascii"))
    time.sleep(0.1)
    sock.settimeout(2.0)
    try:
        return sock.recv(8192).decode("ascii", errors="replace")
    except socket.timeout:
        return ""


def read_byte(sock, addr):
    """Read a byte from C64 memory. Returns int or None."""
    resp = send_cmd(sock, f"m ${addr:04X} ${addr:04X}")
    # Parse hex bytes from response like ">C:c000  ff  ..."
    for line in resp.split("\n"):
        if f"{addr:04x}" in line.lower():
            parts = line.split()
            for part in parts:
                part = part.strip().rstrip(".")
                if len(part) == 2:
                    try:
                        return int(part, 16)
                    except ValueError:
                        continue
    return None


def main():
    prg_path = None
    printer_file = None
    timeout = DEFAULT_TIMEOUT

    i = 1
    while i < len(sys.argv):
        if sys.argv[i] == "--printer" and i + 1 < len(sys.argv):
            printer_file = os.path.abspath(sys.argv[i + 1])
            i += 2
        elif sys.argv[i] == "--timeout" and i + 1 < len(sys.argv):
            timeout = int(sys.argv[i + 1])
            i += 2
        elif sys.argv[i].startswith("-"):
            print(f"Unknown flag: {sys.argv[i]}")
            sys.exit(1)
        else:
            prg_path = os.path.abspath(sys.argv[i])
            i += 1

    if not prg_path:
        print("Usage: python vice_run.py <program.prg> [--printer <file>] [--timeout <sec>]")
        sys.exit(1)

    vice_args = [
        VICE_PATH,
        "-warp",
        "-remotemonitor",
        "-remotemonitoraddress", f"ip4://127.0.0.1:{MONITOR_PORT}",
        "-autostart", prg_path,
    ]
    if printer_file:
        if os.path.exists(printer_file):
            os.remove(printer_file)
        vice_args += [
            "-device4", "1",
            "-iecdevice4",
            "-virtualdev4",
            "-pr4drv", "ascii",
            "-pr4output", "text",
            "-pr4txtdev", "0",
            "-prtxtdev1", printer_file,
        ]

    print(f"[vice_run] Launching VICE (poll interval={timeout}s)...")
    proc = subprocess.Popen(vice_args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    sock = connect_monitor(proc)
    if sock is None:
        die("Could not connect to VICE monitor", proc)

    # Resume execution
    print("[vice_run] Resuming C64 execution...")
    send_cmd(sock, "x")

    # Poll loop: read $C000 every <timeout> seconds.
    # If value changed since last read, keep going.
    # If value is the same, program is done — quit.
    last_val = None
    while True:
        time.sleep(timeout)

        if proc.poll() is not None:
            print("[vice_run] VICE exited on its own.")
            break

        val = read_byte(sock, WATCH_ADDR)
        print(f"[vice_run] ${WATCH_ADDR:04X} = {f'${val:02X}' if val is not None else 'read failed'} (last: {f'${last_val:02X}' if last_val is not None else 'none'})")

        if val is not None and val == last_val:
            print("[vice_run] Value unchanged — program is done.")
            send_cmd(sock, "quit")
            break

        # Resume execution (reading memory paused it)
        send_cmd(sock, "x")
        last_val = val

    sock.close()

    # Wait for VICE to exit
    try:
        rc = proc.wait(timeout=timeout)
        print(f"[vice_run] VICE exited with code {rc}")
    except subprocess.TimeoutExpired:
        print("[vice_run] VICE did not exit, killing...")
        proc.kill()
        proc.wait()
        print("[vice_run] VICE killed.")

    sys.exit(0)


if __name__ == "__main__":
    main()
