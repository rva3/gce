import argparse
import mmap

TARGET_PHYS_ADDR = 0xA8000000
CHUNK_SIZE = 1 * 1024 * 1024


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Stage a kernel image to a specific physical address using /dev/mem."
        )
    )
    parser.add_argument(
        "image_path",
        nargs="?",
        default="Image",
        help=(
            "Path to the kernel image file to stage (default: 'Image' in current"
            " directory)"
        ),
    )
    args = parser.parse_args()

    kernel_image_path = args.image_path

    try:
        with open(kernel_image_path, "rb") as f:
            kernel_code = f.read()
    except FileNotFoundError:
        print(f"Error: The file '{kernel_image_path}' was not found.")
        return
    except Exception as e:
        print(f"Error reading '{kernel_image_path}': {e}")
        return

    remainder = len(kernel_code) % CHUNK_SIZE
    if remainder != 0:
        padding_needed = CHUNK_SIZE - remainder
        kernel_code += b"\x00" * padding_needed

    total_size = len(kernel_code)
    page_size = mmap.PAGESIZE

    print(
        f"Staging total {total_size} bytes (including padding) from"
        f" '{kernel_image_path}' to 0x{TARGET_PHYS_ADDR:X}..."
    )

    try:
        with open("/dev/mem", "r+b") as mem_file:
            bytes_written = 0

            while bytes_written < total_size:
                current_chunk_size = min(CHUNK_SIZE, total_size - bytes_written)
                current_target_addr = TARGET_PHYS_ADDR + bytes_written

                aligned_base = current_target_addr & ~(page_size - 1)
                offset_correction = current_target_addr - aligned_base

                map_length = offset_correction + current_chunk_size
                map_length = (map_length + page_size - 1) & ~(page_size - 1)

                try:
                    with mmap.mmap(
                        mem_file.fileno(),
                        map_length,
                        mmap.MAP_SHARED,
                        mmap.PROT_READ | mmap.PROT_WRITE,
                        offset=aligned_base,
                    ) as mem:
                        chunk = kernel_code[
                            bytes_written : bytes_written + current_chunk_size
                        ]
                        mem[
                            offset_correction : offset_correction + current_chunk_size
                        ] = chunk

                except Exception as e:
                    print(f"Error mapping at 0x{aligned_base:X}: {e}")
                    raise

                bytes_written += current_chunk_size
                print(f"Staged {bytes_written}/{total_size} bytes...")

        print(f"Successfully staged kernel image to 0x{TARGET_PHYS_ADDR:X}")

    except PermissionError:
        print("Error: Permission denied accessing /dev/mem. Are you using SUDO?")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")


if __name__ == "__main__":
    main()
