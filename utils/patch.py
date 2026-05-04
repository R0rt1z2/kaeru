#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2026 Roger Ortiz <me@r0rt1z2.com>
# SPDX-License-Identifier: AGPL-3.0-or-later
#

import struct
from argparse import ArgumentParser
from pathlib import Path

from liblk import LkImage
from liblk.structures import LkPartition


class DeviceConfig:
    def __init__(self, path) -> None:
        self.config = {}
        with open(path) as f:
            for line in f:
                if line.startswith('CONFIG_'):
                    key, value = line.strip().split('=', 1)
                    self.config[key] = value

    def get(self, key) -> str:
        return self.config.get('CONFIG_' + key, None)


def to_int(s) -> int:
    return int(s, 16) if s.startswith('0x') else int(s)


def encode_bl(src, dst):
    off = dst - (src + 4)
    hi, lo = (off >> 12) & 0x7FF, (off >> 1) & 0x7FF
    return struct.pack('<HH', 0xF000 | hi, 0xF800 | lo)


def patch_bss(partition: LkPartition, payload_size: int) -> None:
    bss_start = partition.header.data_size + partition.lk_address
    position = partition.data.find(struct.pack('<I', bss_start))
    dest_addr = (bss_start + payload_size + 3) & ~3

    # This should be fatal regardless of the injection method, because we
    # will always need room for either kaeru or stage1.
    if position < 0:
        exit('ERROR: Unable to find BSS markers!')

    while position > 0:
        partition.data[position : position + 4] = struct.pack('<I', dest_addr)
        position = partition.data.find(struct.pack('<I', bss_start))

    return bss_start


def main() -> None:
    parser = ArgumentParser(
        description='Inject payloads into a MediaTek LK bootloader binary'
    )

    parser.add_argument('config', help='Path to the device configuration file')
    parser.add_argument('input', help='Path to the input LK bootloader binary')
    parser.add_argument('payload', help='Path to the payload binary to inject')
    parser.add_argument(
        '-o', '--output', help='Path to the output patched binary'
    )

    # This refers to stage1, which is used to load the actual kaeru payload.
    # Right now only ARMv7 based LKs can boot kaeru without it, but the idea
    # is to enforce this everywhere, so it'll eventually become mandatory.
    parser.add_argument(
        '-l', '--loader', help='First stage loader for the payload'
    )

    args = parser.parse_args()

    for path in (args.config, args.input, args.payload):
        if not Path(path).is_file():
            exit("ERROR: File not found: '%s'!" % path)

    lk = LkImage(args.input)
    config = DeviceConfig(args.config)
    device = Path(args.config).name.removesuffix('_defconfig')

    base = to_int(config.get('BOOTLOADER_BASE'))
    size = to_int(config.get('BOOTLOADER_SIZE'))
    plic = to_int(config.get('PLATFORM_INIT_CALLER'))

    # We won't go anywhere without a valid base and size, so it's
    # reasonable to check this before doing any actual work.
    if not base or not plic or not size or size <= 0:
        exit('ERROR: Invalid basic required configuration!')

    print('Bootloader base: 0x%X' % base)
    print('Bootloader size: %d bytes' % size)
    print('Platform init caller: 0x%X' % plic)

    # Some devices might use "LK" instead of "lk" for the partition
    # name, so better safe than sorry.
    part = lk.partitions.get('lk') or lk.partitions.get('LK')
    if not part:
        exit("ERROR: No 'lk' partition found in the image!")

    # If this happens, it probably means we're trying to build for
    # an incompatible LK image, so bail out.
    assert part.lk_address == base, 'Wrong load address for LK partition (expected 0x%X, got 0x%X)' % (base, part.lk_address)

    payload = open(args.payload, 'rb').read()
    payload_size = len(payload)
    print('Payload size: %d bytes' % payload_size)

    if args.loader:
        loader = open(args.loader, 'rb').read()
        loader_size = len(loader)
        print('Loader size: %d bytes' % loader_size)

    # Decide if we should increase the code section to make room for
    # the payload, or if we should use a fixed address specified by
    # the configuration file.
    # 
    # Extending the code section is generally more reliable, since a
    # forced address may overlap existing data, which is bad.
    if config.get('FORCE_INJECT_ADDR'):
        payload_dest = to_int(config.get('FORCE_INJECT_ADDR'))
    else:
        payload_dest = patch_bss(
            part, payload_size if not args.loader else loader_size
        )

    # Inject the payload into the end of the 'lk' sub-partition. This
    # can be either stage1 or kaeru as stated before.
    part.data += payload if not args.loader else loader

    if args.loader:
        # If the user specified a loader, this means kaeru has to be
        # stored in a separate sub-partition, so the first stage can
        # easily load it from storage without corrupting anything.
        lk.add_partition(
            name='kaeru',
            data=payload,
            memory_address=0,
            use_extended=True,
        )

    print('Payload destination: 0x%X' % payload_dest)

    # Redirect the platform_init caller to jump into our payload.
    offset = plic - base
    shellcode = encode_bl(plic, payload_dest)

    print('File offset: 0x%X' % offset)
    part.data[offset : offset + len(shellcode)] = shellcode

    lk._rebuild_contents()
    lk.save(args.output if args.output else ('%s-patched.bin' % device))


if __name__ == '__main__':
    main()
