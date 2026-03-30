#!/usr/bin/env python3
"""Capture RND seeds from the actual 10 PRINT program running in VICE.

Autostart the tokenized 10print.prg, set a breakpoint at RND entry ($E097).
Each break means the PREVIOUS call's seed is at $8B-$8F. Read it, continue.

Usage: python vice_sample_rnd.py [--count N]
"""

import sys
import socket
import subprocess
import time
import re
import os

VICE_PATH = r"C:\games\GTK3VICE-3.9-win64\bin\x64sc.exe"
MONITOR_PORT = 6510
PRG_PATH = os.path.join(os.path.dirname(__file__), "..", "c64", "10print.prg")


def drain(sock, timeout=0.5):
    sock.settimeout(timeout)
    chunks = []
    while True:
        try:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data.decode("ascii", errors="replace"))
        except socket.timeout:
            break
    return "".join(chunks)


def send_and_drain(sock, cmd, timeout=1.0):
    sock.sendall((cmd + "\n").encode("ascii"))
    return drain(sock, timeout)


def main():
    count = 10000
    output_file = "vice_basic_seeds.txt"

    for i in range(1, len(sys.argv)):
        if sys.argv[i] == "--count" and i + 1 < len(sys.argv):
            count = int(sys.argv[i + 1])

    prg = os.path.abspath(PRG_PATH)
    print(f"[sample] Will capture {count} seeds from {prg}")

    proc = subprocess.Popen([
        VICE_PATH, "-warp",
        "-remotemonitor",
        "-remotemonitoraddress", f"ip4://127.0.0.1:{MONITOR_PORT}",
        "-autostart", prg,
    ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    # Connect
    sock = None
    for _ in range(30):
        if proc.poll() is not None:
            break
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(2.0)
            sock.connect(("127.0.0.1", MONITOR_PORT))
            drain(sock)
            break
        except (ConnectionRefusedError, socket.timeout, OSError):
            sock.close()
            sock = None
            time.sleep(0.3)

    if sock is None:
        print("[sample] ERROR: Could not connect")
        proc.kill()
        proc.wait()
        sys.exit(1)

    # Set breakpoint at RND entry, then resume.
    # Autostart will load and RUN the program. The breakpoint will fire
    # when the BASIC interpreter calls RND for the first time.
    send_and_drain(sock, "break E097")
    sock.sendall(b"x\n")

    # Collection loop
    fp = open(output_file, "w")
    collected = 0
    hit = 0
    t0 = time.time()

    while collected < count:
        sock.settimeout(5.0)
        try:
            data = sock.recv(4096).decode("ascii", errors="replace")
        except socket.timeout:
            print(f"[sample] Timeout at {collected}")
            break

        if "$e097" not in data.lower():
            continue

        hit += 1
        if hit == 1:
            sock.sendall(b"x\n")
            continue

        # Read seed — read until we see the "008b" line
        sock.sendall(b"m 008B 008F\n")
        sock.settimeout(2.0)
        buf = b""
        seed = None
        while True:
            try:
                buf += sock.recv(4096)
            except socket.timeout:
                break
            text = buf.decode("ascii", errors="replace")
            for line in text.split("\n"):
                if "008b" in line.lower():
                    hexbytes = re.findall(r'\b([0-9a-fA-F]{2})\b', line)
                    if len(hexbytes) >= 5:
                        seed = [int(h, 16) for h in hexbytes[:5]]
                        break
            if seed:
                break

        if seed:
            fp.write(f"{seed[0]:02X} {seed[1]:02X} {seed[2]:02X} {seed[3]:02X} {seed[4]:02X}\n")
            collected += 1

        if collected % 1000 == 0 and collected > 0:
            elapsed = time.time() - t0
            print(f"[sample] {collected}/{count} ({collected/elapsed:.0f}/sec)")

        sock.sendall(b"x\n")

    fp.close()
    elapsed = time.time() - t0
    print(f"[sample] Collected {collected} seeds in {elapsed:.1f}s")

    try:
        sock.sendall(b"quit\n")
        sock.close()
    except:
        pass
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()

    # Compare against C++ simulation
    print(f"\n[sample] Comparing {collected} seeds against C++ simulation...")
    with open("cpp_basic_compare.txt", "w") as f:
        subprocess.run([os.path.join("build", "Release", "gen_seeds.exe"), str(collected)], stdout=f)

    with open("cpp_basic_compare.txt") as f:
        cpp_lines = [" ".join(line.strip().split()[2:]) for line in f]
    with open(output_file) as f:
        vice_lines = [line.strip() for line in f]

    mismatches = 0
    for i, (v, c) in enumerate(zip(vice_lines, cpp_lines)):
        if v != c:
            if mismatches < 10:
                print(f"  MISMATCH at {i+1}: VICE={v} C++={c}")
            mismatches += 1

    if mismatches == 0:
        print(f"  {collected} seeds match perfectly.")
    else:
        print(f"  {mismatches} mismatches out of {min(len(vice_lines), len(cpp_lines))}")


if __name__ == "__main__":
    main()
