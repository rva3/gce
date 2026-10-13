## Foreword
Dear Community,

Today the R&D department of the MediaTek MT6768 department of the MediaTek MT6572 Mainline Inc.
is happy to announce our latest cutting-edge coprocessor research. We've been quietly working for
months, reverse engineering obscure MediaTek MT6768 coprocessors, and we picked well-known yet
obscure target.

Meet Global Command Engine coprocessor. It features a full ISA with control flow and indirect
jumps/calls support, making it a full turing-complete processor. Thanks to various MediaTek
downstream kernels, we've been able to create documentation for this MCU and build an assembler
for custom code execution.

Unfortunately, users of older platforms with legacy and v2 GCEs couldn't use this tooling. Our
internal reverse engineering team informed us that even despite having GPRs in v2 generation, it
still doesn't have any logic operations. Therefore, only v3 remains the feasible target.

We're pleased to publish our research for the Open Source community. This repository includes GCE
assembler & disassembler, RV32IMA interpreter, various RV32IMA and GCE bare-metal demos.

Thanks to Mr. Paxton, who kindly offered help with LLVM port for native GCE ISA, the code
execution speed reaches 5.4 DMIPS, allowing us to run DOOM or other demos which were too slow on
the RV32IMA interpreter.

Please make sure to check out each component README file for better understanding.

## License
Our in-house code is licensed under [AGPLv3](./LICENSE.agpl), documentation (including markdown files) -
under [CC BY NC SA 4.0](./LICENSE.cc). Some demos have their own code, please make sure to check
demos/ directory. Linux kernel patch (drv.diff) is licensed under GPLv2.

## So what?
Dare you ask this kiddo. Our team worked hard to bring up various ASCII demos, video playback,
DOOM and even U-Boot with Linux kernel solely on GCE without any AP help.

Yes, thanks to our talented engineering teams, you can run a real upstream Linux kernel (with few
patches) and U-Boot.

You're not allowed to ask if this is useful. If such question appears, you will be banned on this
repository forever.

***YouTube demo: https://youtu.be/OzFfuF31w54***

## FAQ
### Is there ISA description?
Yes. Check ISA.md file.

### How to start?
You **MUST** install our Kernel Patch for /dev/cmdq interface. You also **MUST** reserve a chunk
of the memory for RISC-V code. Example:
```diff
+               gce_test_mem: gce-test@a8000000 {
+                       reg = <0 0xa8000000 0 0x2000000>;
+                       no-map;
+               };
```
Remember the address. You will need to use it everywhere for the RISC-V guest.

Apply the patch and devicetree change to `https://github.com/mt6768-mainline/linux` ***on the
cleanup branch*** and deploy to your device. Boot it and setup SSH.

### How do I run demos?
Check demos/ directory.

### How do I run DOOM?
Check demos/ directory.

### How do I run Linux?
First, you will need to run `git clone https://github.com/rva3/linux-mtkgcev3 linux`. Then you
will need to run `make ARCH=riscv LLVM=1 -j16 gce_defconfig`. Finally, run
`make ARCH=riscv LLVM=1 -j16` to build `Image`.

You can run linux on both real hardware and simulator. Unless you want to see a TRUE POWER of
MediaTek GCE (15 minutes for the boot :trollface:), using simulator is recommended. Make sure to
check gce/ directory. Otherwise, run `scp linux/arch/riscv/boot/Image root@buildroot.local:Image`.
Then also copy `rv32_on_gce/kstage.py` script. Run it `python kstage.py`. Finally, use
`python mkbin.py kernel -o code.bin` and copy `code.bin` to the host. Launch it with
`dd if=code.bin of=/dev/cmdq bs=16M` and enjoy.

!! NOTE !! You **MUST** zero RISC-V guest memory for each Linux/U-Boot re-run.
Use `dd if=/dev/zero of=zero.bin bs=32M count=1` and `python kstage.py zero.bin`

For demo check gce/ directory.

### How do I run U-Boot?

1. `make ARCH=riscv CROSS_COMPILE=riscv32-unknown-elf- CPU=generic -j16 gcev3_defconfig`
2. `make ARCH=riscv CROSS_COMPILE=riscv32-unknown-elf- CPU=generic -j16`
3. `scp u-boot.bin root@buildroot.local:`
4. `python mkbin.py kernel -o code.bin`
5. `scp code.bin root@buildroot.local:`
6. on device: `python kstage.py u-boot.bin`
7. on device: `dd if=code.bin of=/dev/cmdq bs=16M`
8. on device to stop: `echo stop > /dev/cmdq`
99. check status: `cat /dev/cmdq`

demo:
```
<debug_uart>


U-Boot 2026.10-rc5-00034-g983dd29a2bad-dirty (Oct 13 2026 - 15:54:11 +0300)

DRAM:  16 MiB
Core:  7 devices, 5 uclasses, devicetree: embed
Loading Environment from nowhere... OK
In:    serial@11002000
Out:   serial@11002000
Err:   serial@11002000
=> version
U-Boot 2026.10-rc5-00034-g983dd29a2bad-dirty (Oct 13 2026 - 15:54:11 +0300)

riscv32-unknown-elf-gcc (g6afcc4f6d) 16.1.0
GNU ld (GNU Binutils) 2.47.20260726
=> echo "hello from gce"
"hello from gce"
=>
```

### Does this work on other SoCs?
On later than 6768, yes. For older, it's hard to tell which ones have GCE v3. But *early mt81xx*
and *all 65xx* definitely **WON'T WORK AND WILL NEVER BE SUPPORTED** due to their ISA. They are
***NOT*** turing-complete.

For later chips, such as 6785, ISA has minor changes. You will need to adjust assembler target.rs
for PC shift set to `3` (and LLVM too).

### WhY LeENoX is RiSc-V aNd NoT NatIvE?
Unfortunately, we can't magically add interrupt handling to the GCE. Instead, we must emulate them
in the software.

### WhY LeeNox BoOt TakEs 15 MinUtEs?/??///??
Feel free to tweak kernel config.

### CaN I OvErCloCK GcE??////????///?///???
No.

### GcE HaS 16 ThrEaDs wHy So SlOw?
Read Linux dt-bindings. Let me save you some time:

> The Global Command Engine (GCE) is an instruction based, multi-threaded,
> ***single-core*** command dispatcher for MediaTek hardware.

In other words, only one thread can be executed without blocking others. Moreover, we steal
CPRs (SRAM regs) from the other threads to take advantage of huge register count and reduce
stack usage.

---

Regards,

R&D department of the MediaTek MT6768 department of the MediaTek MT6572 Mainline Inc.

PhD in using obscure ISAs since 2012.
