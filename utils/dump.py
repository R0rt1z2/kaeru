#!/usr/bin/env python3
#
# Taken from https://github.com/arturkow2000/lgk10exploit
# No license specified by the original author of the script
#

import ctypes
import mmap
from argparse import ArgumentParser
from struct import unpack

MAGIC = 0x58881688
EXT_MAGIC = 0x58891689


def dump_header(buffer, offset, debug=False):
    has_ext = False

    magic = unpack('<I', buffer[0:4])[0]
    if magic != MAGIC:
        if debug:
            print(' '.join('{:02x}'.format(x) for x in buffer[:0x20]))
        raise RuntimeError(
            'invalid magic value, expected {:#x} got {:#x}'.format(MAGIC, magic)
        )

    data_size = unpack('<I', buffer[4:8])[0]
    name = buffer[8:40].decode('utf-8')
    addressing_mode = ctypes.c_int(unpack('<I', buffer[40:44])[0]).value
    memory_address = unpack('<I', buffer[44:48])[0]
    image_list_end = 0
    header_size = 512
    alignment = 8

    if unpack('<I', buffer[48:52])[0] == EXT_MAGIC:
        has_ext = True
        header_size = unpack('<I', buffer[52:56])[0]
        header_version = unpack('<I', buffer[56:60])[0]
        image_type = unpack('<I', buffer[60:64])[0]
        image_list_end = unpack('<I', buffer[64:68])[0]
        alignment = unpack('<I', buffer[68:72])[0]
        data_size |= unpack('<I', buffer[72:76])[0] << 32
        memory_address |= unpack('<I', buffer[76:80])[0] << 32

    print('Partition Name:  %s' % name)
    print('Data Size:       %d' % data_size)

    if addressing_mode == -1:
        print('Addressing Mode: NORMAL')
    elif addressing_mode == 0:
        print('Addressing Mode: BACKWARD')
    else:
        print('Addressing Mode: UNKNOWN(%#08x)' % addressing_mode)

    if has_ext:
        if memory_address & 0xFFFFFFFF != 0xFFFFFFFF:
            print('Address:         %#016x' % memory_address)
        else:
            print('Address:         DEFAULT')
    else:
        if memory_address & 0xFFFFFFFF != 0xFFFFFFFF:
            print('Address:         %#08x' % memory_address)
        else:
            print('Address:         DEFAULT')

    if has_ext:
        print('Header Size:     %d' % header_size)
        print('Header Version:  %d' % header_version)
        print('Image Type:      0x%x' % image_type)
        print('Image List End:  %d' % image_list_end)
        print('Alignment:       %d' % alignment)

    if image_list_end:
        return None
    else:
        new_offset = offset + header_size + data_size
        if alignment != 0 and new_offset % alignment != 0:
            new_offset += (alignment - new_offset) % alignment
        return new_offset


def main():
    parser = ArgumentParser()
    parser.add_argument('file')
    parser.add_argument('-d', '--debug', action='store_true')
    args = parser.parse_args()

    file = open(args.file, 'rb')

    buffer = mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ)

    offset = 0
    while True:
        if args.debug:
            print('Offset: %#08x' % (offset + 0x48000000), flush=True)
        n = dump_header(buffer[offset:], offset, args.debug)
        if n == None:
            break
        print('', flush=True)
        assert isinstance(n, int)
        offset = n

    file.close()


if __name__ == '__main__':
    main()
