Gaming department of the MediaTek MT6768 Mainline department of the MediaTek MT6572 Mainline Inc.
is happy to present our newest Gaming Cool E-sports (GCE) platform.

Thanks to our innovative RISC-V emulator on the GCE, the performance of the interpreted code
reaches up to 0.4 DMIPS. This allows smooth playback of ASCII videos, as well as snake and
tetris games.

Our R&D department invented a new UART overclocking technique. By using 6M baudrate, compared to
mediocre 921600 default, combined with the native video playback without RV emulator, we were
able to reach 30 fps playback of the ASCII video. The maximum baudrate depends on the user
hardware and wiring. We recommend FT2232H for the best experience. You may need to decrease the
baudrate to 4M or less for more stable playback. Some dongles (CP2102, CH340) may be limited to
the default 921600 baud. Run the `baud.py` script for setting baud rates higher than 4M on the
device. On the host we recommend using `tio`.

Please note that some of the projects in this directory are external and have their own licenses.
- tetris: https://github.com/troglobit/tetris/ - ISC - adapted for bare-metal RISC-V by MediaTek MT6768 R&D department
- dhrystone: imported from Buildroot - public domain - adapted for bare-metal RISC-V by
MediaTek MT6768 R&D department
- doom-cli: https://github.com/ludocode/doom-cli - GPLv2 - adapter for bare-metal GCE by MediaTek
MT6768 R&D department
- nanoprintf.h - https://github.com/charlesnicholson/nanoprintf - UNLICENSE OR 0BSD - adapted for
C89 by MediaTek MT6768 R&D department

All other files were written using our best in-house programming guidelines.

For the firmware uploading, you will need to install our new Proprietary and Confidential Linux
Kernel driver Patch for the /dev/cmdq interface. Then you will be able to perform `dd if=code.bin of=/dev/cmdq bs=16M` on the device.

### FAQ
- Q: I want to play tetris/snake/measure performance how do I do it????///??&???&?&??
- A: Install our Kernel Patch. Compile Upstream Kernel Fork maintained by the MediaTek MT6572
Mainline Inc for the MediaTek MT6768 SoC. Compile the target project using
`make TARGET=riscv snake.bin` (you will need RV32 toolchain for this). Copy to the device. Run
`dd if=code.bin of=/dev/cmdq bs=16M`
- Q: WhY So SlOw??&??&?&??//
- A: MediaTek MT6768 R&D department continuously improves our codebase quality and performance.
Unfortunately, we can't do anything to speed up this hardware. Our RV32 emulator and assembler
already is maxing out the last bits of performance on this MCU. If you want to use native LLVM
codegen, use `make TARGET=gce *target*.bin` (you will need to build LLVM with our innovative patch
for GCE support).
- Q: CanT yOu jUst PoRt GcC????///6/?&?&?/
- A: MediaTek MT6768 R&D department has limited resources, especially time. If you're interested
in sponsoring our work, please open an issue.

### Build
You can use RISC-V or native GCE versions. GCE code reaches up to 5.4 DMIPS, while RISC-V is 0.4
DMIPS. Moreover, some demos, such as DOOM or native video playback, are available only native code.

To build demos you need something like `make TARGET=riscv cube.bin` or `make TARGET=gce cube.bin`.

To run the doom you will need to download DOOM1.WAD and use `make TARGET=gce doom.bin`.

Best,

Gaming department of the MediaTek MT6768 Mainline department of the MediaTek MT6572 Mainline Inc.
