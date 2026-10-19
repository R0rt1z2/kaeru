#!/usr/bin/env python3
# Script: kaeru-dump.py
# Author: kelexine <https://github.com/kelexine>
# Date: 2026-10-19
# Purpose: High-speed chunked partition dumper for kaeru fastboot on CUBOT Marlon (MT6765)
# Usage: ./kaeru-dump.py [options] <partition> [output_file]

import argparse
import os
import re
import subprocess
import sys
import tempfile
import time


def run_cmd(cmd: list[str]) -> tuple[int, str, str]:
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return res.returncode, res.stdout, res.stderr


def get_var(var: str) -> str | None:
    code, out, err = run_cmd(["fastboot", "getvar", var])
    output = out + err
    for line in output.splitlines():
        if line.startswith(f"{var}:"):
            val = line.split(":", 1)[1].strip()
            return val
    return None


def get_partition_size(partition: str) -> int:
    val = get_var(f"partition-size:{partition}")
    if val:
        try:
            return int(val, 0)
        except ValueError:
            pass

    # Fallback to 'fastboot oem read <partition>'
    code, out, err = run_cmd(["fastboot", "oem", "read", partition])
    output = out + err
    m = re.search(r"Size:\s*(0x[0-9a-fA-F]+|\d+)\s*bytes", output)
    if m:
        return int(m.group(1), 0)

    return 0


def format_bytes(num: float) -> str:
    for unit in ["B", "KB", "MB", "GB"]:
        if abs(num) < 1024.0:
            return f"{num:3.1f} {unit}"
        num /= 1024.0
    return f"{num:.1f} TB"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Fastboot chunked partition dumper for kaeru on CUBOT Marlon (MT6765)"
    )
    parser.add_argument("partition", help="Name of partition to dump (e.g. boot, recovery, super)")
    parser.add_argument("output", nargs="?", help="Output file path (default: <partition>.img)")
    parser.add_argument("--offset", type=lambda x: int(x, 0), default=0, help="Start offset (hex or dec)")
    parser.add_argument("--size", type=lambda x: int(x, 0), default=0, help="Total size to dump (0 = full partition)")
    parser.add_argument("--chunk-size", type=lambda x: int(x, 0), default=0x100000, help="Chunk size (default 1MB)")

    args = parser.parse_args()

    # Verify device presence
    code, out, err = run_cmd(["fastboot", "devices"])
    if not out.strip():
        print("[-] Error: No fastboot device detected.", file=sys.stderr)
        print("    Run 'adb reboot bootloader' first or check USB connection.", file=sys.stderr)
        return 1

    part = args.partition
    outfile = args.output if args.output else f"{part}.img"

    total_size = get_partition_size(part)
    if total_size <= 0:
        print(f"[-] Error: Could not determine size of partition '{part}'.", file=sys.stderr)
        print("    Verify partition name with 'fastboot oem read " + part + "'.", file=sys.stderr)
        return 1

    offset = args.offset
    dump_size = args.size if args.size > 0 else (total_size - offset)
    chunk_size = args.chunk_size

    if offset + dump_size > total_size:
        print(f"[-] Error: Range 0x{offset:X}..0x{offset + dump_size:X} exceeds partition size 0x{total_size:X}.", file=sys.stderr)
        return 1

    print(f"[*] Target Partition : {part}")
    print(f"[*] Partition Size   : 0x{total_size:X} ({format_bytes(total_size)})")
    print(f"[*] Dump Range       : 0x{offset:X} - 0x{offset + dump_size:X} ({format_bytes(dump_size)})")
    print(f"[*] Chunk Size       : 0x{chunk_size:X} ({format_bytes(chunk_size)})")
    print(f"[*] Output File      : {outfile}")
    print("[-] Starting dump...")

    start_time = time.time()
    bytes_done = 0

    with tempfile.NamedTemporaryFile(delete=False, suffix=".kaeru.tmp") as tmp_file:
        tmp_name = tmp_file.name

    try:
        with open(outfile, "wb") as out_f:
            while bytes_done < dump_size:
                cur_off = offset + bytes_done
                cur_len = min(chunk_size, dump_size - bytes_done)

                fetch_arg = f"{part}:0x{cur_off:08x}:0x{cur_len:08x}"
                code, _, err = run_cmd(["fastboot", "fetch", fetch_arg, tmp_name])

                if code != 0:
                    # Retry with decimal format if hex fails
                    fetch_arg_dec = f"{part}:{cur_off}:{cur_len}"
                    code, _, err = run_cmd(["fastboot", "fetch", fetch_arg_dec, tmp_name])

                if code != 0:
                    # Check if file was written despite status code warning
                    if not (os.path.exists(tmp_name) and os.path.getsize(tmp_name) == cur_len):
                        print(f"\n[-] Error fetching chunk @ 0x{cur_off:X} (len 0x{cur_len:X}): {err.strip()}", file=sys.stderr)
                        return 1

                with open(tmp_name, "rb") as cf:
                    chunk_data = cf.read()
                    out_f.write(chunk_data)

                bytes_done += len(chunk_data)

                elapsed = time.time() - start_time
                speed = (bytes_done / elapsed) if elapsed > 0 else 0
                pct = (bytes_done / dump_size) * 100.0

                bar_len = 30
                filled = int(bar_len * (bytes_done / dump_size))
                bar = "=" * filled + "-" * (bar_len - filled)

                sys.stdout.write(
                    f"\r[{bar}] {format_bytes(bytes_done)} / {format_bytes(dump_size)} "
                    f"({pct:5.1f}%) - {format_bytes(speed)}/s"
                )
                sys.stdout.flush()

        elapsed = time.time() - start_time
        avg_speed = (bytes_done / elapsed) if elapsed > 0 else 0
        print(f"\n[+] Successfully dumped {format_bytes(bytes_done)} to '{outfile}' in {elapsed:.2f}s ({format_bytes(avg_speed)}/s)")
        return 0

    finally:
        if os.path.exists(tmp_name):
            os.remove(tmp_name)


if __name__ == "__main__":
    sys.exit(main())
