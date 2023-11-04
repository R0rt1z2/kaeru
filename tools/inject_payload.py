import struct
from argparse import ArgumentParser

from get_load_addr import get_load_address


def thumb_bl_encoding(current_address: int, target_address: int) -> bytes:
    '''
    Encode a Thumb BL instruction.
    :param current_address: The current address.
    :param target_address: The target address.
    :return: The encoded BL instruction.
    '''
    offset = target_address - (current_address + 4)

    high_offset = (offset >> 12) & 0x7FF
    low_offset = (offset >> 1) & 0x7FF

    high_instruction = 0xF000 | high_offset
    low_instruction = 0xF800 | low_offset

    bl_instruction = (low_instruction << 16) | high_instruction
    return bl_instruction.to_bytes(4, byteorder='little')


def inject_payload(lk: str, payload: str,
                   payload_address: int, payload_caller: int, output: str) -> str:
    '''
    Inject a payload into an LK file.
    :param lk: Path to the LK file.
    :param payload: Path to the payload.
    :param address: Address to inject the payload at.
    :param output: Path to the output file.
    :return: The path to the output file.
    '''
    body = bytearray()
    load_address = get_load_address(lk)

    # Craft a custom LK header so preloader loads the whole image
    # (with our payload) into RAM. We use (LK size - 0x200).
    header = struct.pack('<I', 0x58881688) # LK magic.
    header += struct.pack('<I', (0x80000 - 0x200))

    with open(lk, 'rb') as lk_fh:
        # Start by copying our custom header.
        body += header

        # Skip the original header.
        lk_fh.seek(len(header))

        # Copy the rest of the LK.
        body += lk_fh.read()
        lk_size = lk_fh.tell()

    # Patch the payload caller. We use (payload caller + 0x200)
    # because the file includes the LK header.
    caller_offset = (payload_caller + 0x200) - load_address

    # Ideally, the original caller would be a BL(X) instruction.
    # Our shellcode is equal to 'bl payload_address'. It's recommended
    # to use the call to the original app() as the payload caller.
    shellcode = thumb_bl_encoding(payload_caller, payload_address)
    body[caller_offset:caller_offset + len(shellcode)] = shellcode

    lk_size += load_address
    payload_address += 0x200
    assert payload_address >= lk_size, (
        'Payload address (0x%x) must be greater than or'
        ' equal to the LK size (0x%x)' % (payload_address, lk_size)
    )

    # Pad the payload with zeroes. This is the stack and it will be
    # overwritten by the original LK.
    body += b'\x00' * (payload_address - lk_size)

    print("[*] LK load address: 0x%x" % load_address)
    print("[*] Payload address: 0x%x" % payload_address)
    print("[*] Payload caller: 0x%x" % payload_caller)

    with open(payload, 'rb') as payload_fh:
        body += payload_fh.read()

    with open(output, 'wb') as output_fh:
        output_fh.write(body)

    print("[+] Injected payload (%d bytes) to %s!" % (len(body), output))
    return output

def main():
    parser = ArgumentParser()
    parser.add_argument('lk', help='LK file')
    parser.add_argument('payload', help='Payload file')
    parser.add_argument('payload_address', help='Payload address')
    parser.add_argument('caller_address', help='Payload caller address')
    parser.add_argument('-o', '--output', help='Output file')
    args = parser.parse_args()

    if not args.output:
        args.output = args.lk + '.payload'

    inject_payload(args.lk, args.payload, int(args.payload_address, 16),
                   int(args.caller_address, 16), args.output)

if __name__ == '__main__':
    main()