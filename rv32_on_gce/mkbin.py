#!/usr/bin/env python3
import argparse
from pathlib import Path
import re
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
REPO_ROOT = HERE.parent
EMU_ASM = HERE / "emu.asm"
GCE_DIR = REPO_ROOT / "gce"


def get_cpr(idx: int) -> str:
    return f"cpr{idx % 4}t{idx // 4}"


def assemble_to_bin(asm_text: str, out_bin: Path):
    out_bin = out_bin.resolve()
    out_bin.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.NamedTemporaryFile("w", suffix=".asm", delete=False) as tmp:
        tmp.write(asm_text)
        tmp_path = Path(tmp.name)

    try:
        cargo_bin = GCE_DIR / "target" / "release" / "asm"
        if cargo_bin.is_file():
            cmd = [str(cargo_bin), str(tmp_path), str(out_bin), "--raw"]
        else:
            cmd = [
                "cargo",
                "run",
                "--release",
                "-q",
                "--bin",
                "asm",
                "--",
                str(tmp_path),
                str(out_bin),
                "--raw",
            ]

        r = subprocess.run(cmd, cwd=str(GCE_DIR), capture_output=True, text=True)
        if r.returncode != 0:
            raise RuntimeError(
                f"Assembler failed:\nSTDOUT:\n{r.stdout}\nSTDERR:\n{r.stderr}"
            )
    finally:
        tmp_path.unlink(missing_ok=True)


def build_cpr_init(stack_top: int, gp: int, a1: int = 0) -> list[str]:
    """Initialize registers (x2=sp, x3=gp, x11=a1, others zeroed)."""
    lines = ["init:"]
    for i in range(64):
        cpr = get_cpr(i)
        if i == 2:
            val = stack_top
        elif i == 3:
            val = gp
        elif i == 11:
            val = a1
        else:
            val = 0
        lines.append(f"  ldi {cpr}, #0x{val:x}")
    return lines


def cmd_elf(args):
    elf = args.elf.resolve()

    hdr = subprocess.check_output(["llvm-readelf", "-h", str(elf)], text=True)
    m_entry = re.search(r"Entry point address:\s+0x([0-9a-fA-F]+)", hdr)
    if not m_entry:
        raise ValueError(f"Could not find entry point in {elf}")
    entry = int(m_entry.group(1), 16)

    nm = subprocess.run(["llvm-nm", str(elf)], capture_output=True, text=True)
    gp_m = re.search(r"([0-9a-fA-F]+)\s+\S\s+__global_pointer\$", nm.stdout)
    gp = int(gp_m.group(1), 16) if gp_m else 0

    raw = subprocess.check_output(["llvm-objcopy", "-O", "binary", str(elf), "-"])
    raw += b"\x00" * ((4 - len(raw) % 4) % 4)

    lines = [
        f".base 0x{args.gce_base:x}",
        "stage:",
        f"  ldi r2, #0x{args.guest_base:x}",
    ]
    for i in range(0, len(raw), 4):
        w = int.from_bytes(raw[i : i + 4], "little")
        lines.extend([f"  ldi r0, #0x{w:08x}", "  write [r2], r0", "  add r2, r2, #4"])

    lines.extend(build_cpr_init(args.stack_top, gp))
    lines.extend([
        "  ldi cpr2t8, #0x1800",
        f"  ldi r15, #0x{entry:x}",
        "  ldi r0, #0x0",
        "  jump fetch",
    ])

    full_asm = "\n".join(lines) + "\n" + EMU_ASM.read_text()
    assemble_to_bin(full_asm, args.out)
    print(f"packed ELF to {args.out} ({len(raw)}B payload, entry=0x{entry:x})")


def cmd_kernel(args):
    lines = [
        f".base 0x{args.gce_base:x}",
    ]
    lines.extend(build_cpr_init(args.stack_top, gp=0, a1=args.dtb_addr))

    lines.extend([
        "  ldi cpr2t8, #0x1800",
        f"  ldi r15, #0x{args.entry:x}",
        "  ldi r0, #0x0",
        "  jump fetch",
    ])

    full_asm = "\n".join(lines) + "\n" + EMU_ASM.read_text()
    assemble_to_bin(full_asm, args.out)
    print(f"packed kernel stub {args.out} (entry=0x{args.entry:x})")


def main():
    p = argparse.ArgumentParser()
    sub = p.add_subparsers(dest="mode", required=True)

    # --- elf ---
    p_elf = sub.add_parser("elf", help="pack ELF")
    p_elf.add_argument("elf", type=Path, help="Input ELF file")
    p_elf.add_argument("-o", "--out", type=Path, required=True, help="Output .bin file")
    p_elf.add_argument("--gce-base", type=lambda x: int(x, 0), default=0xF7600000)
    p_elf.add_argument("--guest-base", type=lambda x: int(x, 0), default=0xA8000000)
    p_elf.add_argument("--stack-top", type=lambda x: int(x, 0), default=0xA8A00000)
    p_elf.set_defaults(func=cmd_elf)

    # --- prestaged ---
    p_kern = sub.add_parser("kernel", help="make stub launcher for pre-staged image")
    p_kern.add_argument(
        "-o", "--out", type=Path, required=True, help="Output .bin file"
    )
    p_kern.add_argument("--gce-base", type=lambda x: int(x, 0), default=0xF7600000)
    p_kern.add_argument("--entry", type=lambda x: int(x, 0), default=0xA8000000)
    p_kern.add_argument("--dtb-addr", type=lambda x: int(x, 0), default=0xA87F0000)
    p_kern.add_argument("--stack-top", type=lambda x: int(x, 0), default=0xA8A00000)
    p_kern.set_defaults(func=cmd_kernel)

    args = p.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
