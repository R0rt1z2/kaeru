#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
# SPDX-License-Identifier: AGPL-3.0-or-later
#

import argparse
import os
import struct

hdr_sz = 0x200
alignment = 16
MAGIC = 0x58881688


def encode_bl(src, dst):
    off = dst - (src + 4)
    hi, lo = (off >> 12) & 0x7FF, (off >> 1) & 0x7FF
    return struct.pack('<HH', 0xF000 | hi, 0xF800 | lo)


def get_cfg(path, key):
    with open(path) as f:
        for line in f:
            if line.startswith('CONFIG_' + key + '='):
                return line.strip().split('=', 1)[1]
    return None


def find_bss_start(data, size, base, hdr_sz):
    pos = 0x12C + 4

    offset = data[pos:].find(bytes.fromhex('704700BF'))
    if offset == -1:
        return None, None

    pos += offset

    j = pos - 4
    while j >= 0x12C + 4 and data[j : j + 4] == b'\0\0\0\0':
        j -= 4

    if j >= 0x12C + 4:
        j -= 4
        bss_pos = j - hdr_sz
        bss_val = struct.unpack('<I', data[j : j + 4])[0]
        return bss_pos, bss_val

    return None, None


def main():
    parser = argparse.ArgumentParser(
        description='Patch a bootloader image with kaeru.'
    )
    parser.add_argument(
        'defconfig', help="Path to the device's defconfig file."
    )
    parser.add_argument('input_image', help='Path to input bootloader file.')
    parser.add_argument('payload', help='Path to the compiled kaeru payload.')
    parser.add_argument(
        '-o',
        '--output',
        help='Path to output patched file',
        default='lk.patched',
    )
    args = parser.parse_args()

    with open(args.input_image, 'rb') as f:
        data = bytearray(f.read())
        size = len(data)

    with open(args.payload, 'rb') as f:
        payload = f.read()
        payload_len = len(payload)

    try:
        base = int(get_cfg(args.defconfig, 'BOOTLOADER_BASE'), 16)
        pivot = int(get_cfg(args.defconfig, 'PLATFORM_INIT_CALLER'), 16)
    except:
        exit('unable to read base or pivot address from defconfig')

    magic, code_sz = struct.unpack('<II', data[:8])
    original_code_sz = code_sz
    name = data[8:40].decode('utf-8').rstrip('\0')

    bss_start_pos, bss_start = find_bss_start(data, size, base, hdr_sz)
    if bss_start is None:
        exit('unable to find bss start address')

    original_bss_start = bss_start
    inject_addr = (bss_start - base + hdr_sz) & ~1
    bss_start = (bss_start + payload_len + 3) & ~3

    for i in (bss_start_pos - 4, bss_start_pos):
        data[i + hdr_sz : i + 4 + hdr_sz] = struct.pack('<I', bss_start)
    print('bss start: 0x%08x -> 0x%08x' % (original_bss_start, bss_start))

    code_sz += payload_len
    data[:8] = struct.pack('<II', magic, code_sz)

    print('code size: %d -> %d' % (original_code_sz, code_sz))
    print('payload injection point: 0x%08x' % ((base + inject_addr) - hdr_sz))

    p1 = data[:inject_addr] + payload
    rem = len(p1) % alignment
    pad = 0 if rem == 0 else alignment - rem

    print('payload padding: %d bytes' % pad)
    p2 = (b'\0' * pad) + data[inject_addr:]
    out = p1 + p2

    tot_len = hdr_sz + code_sz
    aligned = ((tot_len + alignment - 1) // alignment) * alignment
    end_pad = aligned - tot_len

    print('partition end padding: %d bytes' % end_pad)
    if end_pad > 0:
        out += b'\0' * end_pad

    shellcode = encode_bl(pivot, (base + (inject_addr - hdr_sz)) | 1)
    offset = (pivot - base) + hdr_sz
    out[offset : offset + len(shellcode)] = shellcode

    part_end = hdr_sz + code_sz
    aligned_pos = ((part_end + alignment - 1) // alignment) * alignment
    fixed = out[:aligned_pos]

    mg_bytes = struct.pack('<I', MAGIC)

    if len(out) > aligned_pos + 4:
        mg_pos = out.find(mg_bytes, aligned_pos)

        if mg_pos > aligned_pos:
            pad_size = mg_pos - aligned_pos
            if all(b == 0 for b in out[aligned_pos:mg_pos]):
                print('unaligned partition padding: %d bytes' % pad_size)
                fixed += out[mg_pos:]
            else:
                fixed += out[aligned_pos:]
        else:
            fixed += out[aligned_pos:]

    next_pos = ((hdr_sz + code_sz + alignment - 1) // alignment) * alignment
    print('final next partition offset: 0x%x' % next_pos)

    with open(args.output, 'wb') as f:
        f.write(fixed)

    print('patched bootloader written to %s!' % args.output)


if __name__ == '__main__':
    main()
