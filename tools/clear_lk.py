import struct
from argparse import ArgumentParser

CHUNK_SIZE = 0x400

def clear_lk(input_path: str, output_path: str = 'lk.clear') -> str:
    """
    Remove extraneous headers from LK file.
    :param input_path: The input LK image.
    :param output_path: The output file.
    :return: The path to the output file if changes were made, else None.
    """
    try:
        with open(input_path, 'rb') as input_fh:
            buf = input_fh.read(CHUNK_SIZE)
            offset = 0x4040  # Default to 'BFBF'

            if buf[0:4] == b'SSSS':
                offset = struct.unpack("<I", buf[44:48])[0]
            else:
                if buf[0:4] != b'BFBF':
                    exit("[!] %s is not a signed image." % input_path)

            with open(output_path, 'wb') as output_fh:
                input_fh.seek(offset)
                output_fh.write(input_fh.read())

        print("[*] Cleared %s header (offset: 0x%x) to %s" % (buf[0:4].decode('utf-8'),
                                                              offset, output_path))
        return output_path
    except IOError as e:
        exit("[!] %s" % e)

def main():
    parser = ArgumentParser(description='Remove extraneous headers from LK file')
    parser.add_argument('input', help='LK file')
    parser.add_argument('-o', '--output', help='Output file')
    args = parser.parse_args()

    clear_lk(args.input, args.output)

if __name__ == '__main__':
    main()
