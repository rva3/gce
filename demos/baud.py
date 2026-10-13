import argparse
import ctypes
import fcntl

TCGETS2 = 0x802C542A
TCSETS2 = 0x402C542B
BOTHER  = 0x1000
CBAUD   = 0x100F

class Termios2(ctypes.Structure):
    _fields_ = [
        ('c_iflag', ctypes.c_uint),
        ('c_oflag', ctypes.c_uint),
        ('c_cflag', ctypes.c_uint),
        ('c_lflag', ctypes.c_uint),
        ('c_line', ctypes.c_ubyte),
        ('c_cc', ctypes.c_ubyte * 19),
        ('c_ispeed', ctypes.c_uint),
        ('c_ospeed', ctypes.c_uint),
    ]

def set_custom_baudrate(fd: int, baudrate: int) -> None:
    """Sets an arbitrary baud rate using Linux termios2 ioctl."""
    t2 = Termios2()
    fcntl.ioctl(fd, TCGETS2, t2)

    t2.c_cflag = (t2.c_cflag & ~CBAUD) | BOTHER
    t2.c_ispeed = baudrate
    t2.c_ospeed = baudrate

    fcntl.ioctl(fd, TCSETS2, t2)

def main():
    parser = argparse.ArgumentParser(description="Test/set UART baud for mtk8250")
    parser.add_argument("baudrate", type=int, default=6000000, help="Target baud rate")
    parser.add_argument("--device", default="/dev/ttyS0", help="Serial port device")
    parser.add_argument("--size", type=int, default=1 << 20, help="Total payload size in bytes")
    args = parser.parse_args()

    payload = bytes(range(256)) * (args.size // 256)

    with open(args.device, 'r+b', buffering=0) as dev:
        set_custom_baudrate(dev.fileno(), args.baudrate)
        written = dev.write(payload)
        print(f"Successfully sent {written} bytes to {args.device} at {args.baudrate} baud.")

if __name__ == "__main__":
    main()
