def rotate_left(x: int, n: int=1, width: int=8) -> int:
    """ Rotate left an integer.

    Arguments:
        x: The integer to rotate.
        n: the number of positions to rotate.
        width: the number of bits of the input integer.
    """
    part1 = x << (n % width)
    part2 = x >> ((width - n) % width)
    return (part1 | part2) & ((1 << width) - 1)

def rotate_left_dword(dword: int, n: int) -> int:
    """ Rotate left a 32-bit integer.

    Arguments:
        x: The integer to rotate.
        n: the number of positions to rotate.
    """
    num_bits_in_dword = 32
    return rotate_left(x=dword, n=n, width=num_bits_in_dword)

def swap_endianness_per_4_bytes(data: bytes) -> bytes:
    """Swap endianness in a bytes object, 4 bytes at the time."""
    num = len(data) // 4
    return struct.pack(f'>{num}I', *struct.unpack(f'<{num}I', data))
