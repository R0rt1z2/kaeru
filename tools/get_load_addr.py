from argparse import ArgumentParser

from liblk.LkImage import LkImage

def get_load_address(image: str) -> int:
    '''
    Gets the load address from the given LK image.
    :param image: The LK image to get the load address from.
    :return: The load address.
    '''
    image = LkImage(image)
    partition = image.get_partition_by_name('lk')
    return partition.header.memory_address

def main():
    parser = ArgumentParser()
    parser.add_argument('input')
    args = parser.parse_args()
    print(hex(get_load_address(args.input)))

if __name__ == '__main__':
    main()
