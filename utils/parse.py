#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
# SPDX-License-Identifier: AGPL-3.0-or-later
#

import mmap
import os
import re
import struct
from argparse import ArgumentParser

from capstone import *

"""
Pattern signatures for LK (Little Kernel) bootloader function identification.

This dictionary contains byte patterns that uniquely identify key functions
within MediaTek LK bootloader binaries. Each pattern is a sequence of hex bytes
where 'XX' acts as a wildcard for variable bytes.

Export behavior:
- export=True: Function addresses are written to defconfig as CONFIG_* entries
  These are primary targets needed for kaeru to function properly.
- export=False: Used internally during analysis but not exported to config
  These serve as stepping stones to find other functions or provide manual hints
  (e.g., bootstrap2 helps locate platform_init_caller, fastboot_continue provides
  bootmode hints)
"""
patterns = {
    'app': {
        'patterns': [
            '64 48 f8 b5 64 4e 78 44 de f7 34 fb',
            '42 48 70 b5 42 4d',
            'b8 4b 2d e9 f0 4f',
            '49 48 f8 b5 49 4e',
            '2d e9 f0 XX XX b0 6a 4e',
        ],
        'export': True,
    },
    'app_caller': {'patterns': ['98 47 b5 42 f8 d1'], 'export': True},
    'fastboot_continue': {
        'patterns': ['15 4a 00 21 08 b5 15 4b 7a 44'],
        'export': False,
    },
    'fastboot_fail': {
        'patterns': ['01 46 02 48 78 44 ff f7 XX bf'],
        'export': True,
    },
    'fastboot_info': {
        'patterns': ['1a 4a 70 b5 7a 44', 'XX 4a 7a 44 12 68 11 68'],
        'export': True,
    },
    'fastboot_okay': {
        'patterns': ['01 46 02 48 78 44 ff f7 XX be'],
        'export': True,
    },
    'fastboot_publish': {
        'patterns': ['38 b5 05 46 0c 20 0c 46'],
        'export': True,
    },
    'fastboot_register': {
        'patterns': ['2d e9 f0 41 05 46 18 20'],
        'export': True,
    },
    'lk_log_store': {
        'patterns': ['2d e9 f0 41 82 b0 XX 4c 7c'],
        'export': True,
    },
    'mtk_detect_key': {
        'patterns': ['47 28 16 d8 08 28 10 b5', '38 b5 04 46 ff f7 14 ff'],
        'export': True,
    },
    'platform_init': {
        'patterns': ['81 48 2d e9 f0 4f 87 b0', '2d e9 f0 4f 00 20 XX b0'],
        'export': True,
    },
    'bootstrap2': {
        'patterns': [
            '48 10 b5 78 44 XX f0 XX XX XX 4b',
            '08 b5 ff f7 38 ea df f7 e3 fb',
        ],
        'export': False,
    },
    'video_printf': {'patterns': ['0f b4 30 b5 c9 b0 4c ab'], 'export': True},
    'thread_create': {
        'patterns': ['2D E9 F8 43 81 46 64 20 88 46 17 46 1D 46'],
        'export': True,
    },
    'thread_resume': {
        'patterns': [
            '02 68 47 F2 64 23 C7 F2 68 43 30 B5 05 46',
            'f8 b5 83 69 01 3b 01 2b 2d d9 18 4d 04 46',
        ],
        'export': True,
    },
}


def p2r(p):
    return re.compile(
        bytes(
            ''.join('.' if b == 'XX' else '\\x%s' % b for b in p.split()),
            encoding='latin1',
        ),
        re.DOTALL,
    )


def get_load_addr(lk):
    lk.seek(512)
    while (data := lk.read(4)) and data != b'\x10\xff\x2f\xe1':
        if len(data) < 4:
            return None
    if not data:
        return None
    return struct.unpack('<I', lk.read(4))[0] if lk.read(4) else None


def find_offsets(lk, patterns, base):
    results = {}
    for name, data in patterns.items():
        pats = data['patterns']
        export = data['export']
        fo = []
        for pattern in pats:
            fo.extend(
                [match.start() + base for match in p2r(pattern).finditer(lk)]
            )
        if fo:
            results[name] = {'offset': fo[0], 'export': export}
            print('CONFIG_%s=0x%X' % (name.upper(), fo[0]))
    return results


def find_caller(lk, bfn, pi, base, types=('bl', 'blx')):
    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    bfn &= ~1
    for a, _, m, o in md.disasm_lite(lk[bfn - base : bfn - base + 256], bfn):
        if (
            any(m.startswith(t) for t in types)
            and o.startswith('#')
            and int(o.strip('#'), 0) == pi
        ):
            return a
    return None


def read_cfg(filename):
    cfg = {}
    if os.path.exists(filename):
        with open(filename, 'r') as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith('#') and '=' in line:
                    key, value = line.split('=', 1)
                    cfg[key] = value
    return cfg


def write_cfg(filename, cfg):
    with open(filename, 'w') as f:
        for key, value in sorted(cfg.items()):
            f.write('%s=%s\n' % (key, value))


def find_string(lk, string):
    return lk.find(bytes(string, 'latin1'))


def main():
    parser = ArgumentParser()
    parser.add_argument('lk', help='LK image file')
    parser.add_argument('defconfig', help='Output defconfig file')
    args = parser.parse_args()

    cfg = read_cfg(args.defconfig)

    with open(args.lk, 'rb') as fp:
        lk = mmap.mmap(fp.fileno(), 0, access=mmap.ACCESS_READ)

    base = get_load_addr(lk)
    if not base:
        return
    print('CONFIG_BOOTLOADER_BASE=0x%X' % base)

    cfg['CONFIG_BOOTLOADER_BASE'] = '0x%X' % base

    lk.seek(0)
    magic, code_sz = struct.unpack('<II', lk.read(8))
    print('CONFIG_BOOTLOADER_SIZE=0x%X' % code_sz)
    cfg['CONFIG_BOOTLOADER_SIZE'] = '0x%X' % code_sz

    offsets = find_offsets(lk, patterns, base - 0x200)

    if 'bootstrap2' in offsets and 'platform_init' in offsets:
        caller = find_caller(
            lk,
            offsets['bootstrap2']['offset'],
            offsets['platform_init']['offset'],
            base - 0x200,
        )
        if caller:
            print('CONFIG_PLATFORM_INIT_CALLER=0x%X' % caller)
            cfg['CONFIG_PLATFORM_INIT_CALLER'] = '0x%X' % caller

    # Unfortunately there's no easy way to dynamically find the bootmode
    # address, so we have to let the user know where to look for it.
    if 'fastboot_continue' in offsets:
        print(
            '# CONFIG_BOOTMODE_ADDRESS="Go to 0x%X to find the bootmode value"'
            % offsets['fastboot_continue']['offset']
        )

    if find_string(lk, 'LK_LOG_STORE') == -1:
        if 'lk_log_store' in offsets:
            del offsets['lk_log_store']
        print('CONFIG_LK_LOG_STORE=n')
        cfg['CONFIG_LK_LOG_STORE'] = 'n'

    for name, data in offsets.items():
        addr = data['offset']
        export = data['export']
        if export:
            key = 'CONFIG_%s%s' % (
                name.upper(),
                '_ADDRESS' if 'caller' not in name else '',
            )
            cfg[key] = '0x%X' % addr

    write_cfg(args.defconfig, cfg)

    print('#')
    print('# WARNING: These addresses are automatically detected and may not be 100% accurate.')
    print('# Some functions might be missing or incorrectly identified.')
    print('# Please manually review this defconfig and follow the wiki for verification.')
    print('#')


if __name__ == '__main__':
    main()
