#!/usr/bin/env python3
import argparse
from pathlib import Path
from importlib.machinery import SourceFileLoader

HERE = Path(__file__).resolve().parent
DEMOS = HERE.parent
DOOMGENERIC = DEMOS / "doom-cli" / "doomgeneric"
LLVM_ROOT = DEMOS.parent / "gce" / "llvm"

if not LLVM_ROOT.exists():
    LLVM_ROOT = HERE / ".." / ".." / "llvm"

ALT = DEMOS / ".." / "llvm"
if (
    not (LLVM_ROOT / "tools" / "build-image.py").exists()
    and (ALT / "tools" / "build-image.py").exists()
):
    LLVM_ROOT = ALT
builder = SourceFileLoader(
    "gce_build_image", str(LLVM_ROOT / "tools" / "build-image.py")
).load_module()

DOOM_SRCS = """dummy am_map doomdef doomstat dstrings d_event d_items d_iwad d_loop
d_main d_mode d_net f_finale f_wipe g_game hu_lib hu_stuff info i_cdmus i_endoom
i_joystick i_scale i_sound i_system i_timer memio m_argv m_bbox m_cheat m_config
m_controls m_fixed m_menu m_misc m_random p_ceilng p_doors p_enemy p_floor p_inter
p_lights p_map p_maputl p_mobj p_plats p_pspr p_saveg p_setup p_sight p_spec
p_switch p_telept p_tick p_user r_bsp r_data r_draw r_main r_plane r_segs r_sky
r_things sha1 sounds statdump st_lib st_stuff s_sound tables v_video wi_stuff
w_checksum w_file w_main w_wad z_zone w_file_stdc i_input i_video doomgeneric
cli_data""".split()


def ensure_wad():
    wad_src = DEMOS / "doom-cli" / "DOOM1.WAD"
    wad_c = HERE / "wad.c"
    if not wad_src.exists():
        # try lowercase
        alt = DEMOS / "doom-cli" / "doom1.wad"
        if alt.exists():
            wad_src = alt
        else:
            raise SystemExit(f"missing IWAD: {wad_src} (download shareware DOOM1.WAD)")
    data = wad_src.read_bytes()
    print(f"IWAD {wad_src}: {len(data)} bytes", flush=True)

    marker = f"const unsigned int doom_wad_len = {len(data)}u;"
    if wad_c.exists() and marker in wad_c.read_text()[-200:]:
        print(f"{wad_c} up to date", flush=True)
        return
    print(
        f"generating {wad_c} ({len(data) * 5 // 1024 // 1024}MB source)...", flush=True
    )
    with wad_c.open("w") as f:
        f.write("/* Generated from DOOM1.WAD, do not edit */\n")
        f.write("const unsigned char doom_wad[] = {\n")
        for i in range(0, len(data), 16):
            chunk = data[i : i + 16]
            f.write(",".join(f"0x{b:02x}" for b in chunk))
            f.write(",\n")
        f.write("};\n")
        f.write(marker + "\n")
    print("wad.c written", flush=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("-o", "--output", type=Path, default=HERE / "out")
    p.add_argument("--base", type=lambda s: int(s, 0), default=0xF7600000)
    p.add_argument("--capacity", type=lambda s: int(s, 0), default=0x1000000)
    p.add_argument("--stack", type=lambda s: int(s, 0), default=0x10000)
    p.add_argument("-O", "--optimize", default="2", choices=["0", "1", "2", "3", "s"])
    p.add_argument(
        "--verify", action="store_true", help="assemble roundtrip check (slow)"
    )
    a = p.parse_args()
    ensure_wad()
    srcs = [DOOMGENERIC / (n + ".c") for n in DOOM_SRCS]
    srcs += [HERE / "gce_libc.c", HERE / "doomgeneric_gce.c", HERE / "wad.c"]
    for s in srcs:
        if not s.exists():
            raise SystemExit(f"missing source {s}")
    cflags = [
        "-DDOOMGENERIC_RESX=320",
        "-DDOOMGENERIC_RESY=200",
        "-DDISABLE_ZENITY",
        "-DDOOM_CLI",
        "-I" + str(HERE / "include"),
        "-I" + str(DOOMGENERIC),
    ]
    elf, raw = builder.build_image(
        srcs,
        a.output,
        build=LLVM_ROOT / "build",
        base=a.base,
        capacity=a.capacity,
        stack=a.stack,
        optimize=a.optimize,
        verify=a.verify,
        extra_cflags=cflags,
    )
    print(f"built {raw} ({raw.stat().st_size} bytes) base={a.base:#x}", flush=True)


if __name__ == "__main__":
    main()
