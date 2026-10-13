#!/usr/bin/env python3
"""
Convert video frames to an ANSI delta stream C header for bare-metal playback.
Uses `ascii-image-converter -C` to extract 256-color ASCII/Braille cells.
"""

import argparse
import glob
import os
import pty
import re
import struct
import subprocess
import sys
import termios
import time
import fcntl


def _xterm_256_palette():
    pal = [
        (0, 0, 0),
        (128, 0, 0),
        (0, 128, 0),
        (128, 128, 0),
        (0, 0, 128),
        (128, 0, 128),
        (0, 128, 128),
        (192, 192, 192),
        (128, 128, 128),
        (255, 0, 0),
        (0, 255, 0),
        (255, 255, 0),
        (0, 0, 255),
        (255, 0, 255),
        (0, 255, 255),
        (255, 255, 255),
    ]
    levels = (0, 95, 135, 175, 215, 255)
    for r in range(6):
        for g in range(6):
            for b in range(6):
                pal.append((levels[r], levels[g], levels[b]))
    for i in range(24):
        v = 8 + 10 * i
        pal.append((v, v, v))
    return pal


_XTERM_PALETTE = _xterm_256_palette()


def rgb_to_256(r, g, b):
    best, best_d = 0, None
    for i, (pr, pg, pb) in enumerate(_XTERM_PALETTE):
        d = (r - pr) ** 2 + (g - pg) ** 2 + (b - pb) ** 2
        if best_d is None or d < best_d:
            best_d, best = d, i
            if d == 0:
                break
    return best


def _parse_sgr(params, cur_color):
    i = 0
    while i < len(params):
        p = params[i]
        if p == 38 and i + 1 < len(params):
            if params[i + 1] == 5 and i + 2 < len(params):
                cur_color = params[i + 2] % 256
                i += 3
                continue
            if params[i + 1] == 2 and i + 4 < len(params):
                cur_color = rgb_to_256(params[i + 2], params[i + 3], params[i + 4])
                i += 5
                continue
        i += 1
    return cur_color


def extract_frame_cells(
    png_path, cols=80, rows=24, braille=False, dither=False, threshold=60
):
    master, slave = pty.openpty()
    winsize = struct.pack("HHHH", rows, cols, 0, 0)
    fcntl.ioctl(slave, termios.TIOCSWINSZ, winsize)
    env = dict(os.environ, TERM="xterm-256color")
    env.pop("COLORTERM", None)

    cmd = ["ascii-image-converter", png_path, "-C"]
    if braille:
        cmd.append("-b")
        if dither:
            cmd.append("--dither")
        elif threshold is not None:
            cmd += ["--threshold", str(threshold)]

    p = subprocess.Popen(cmd, env=env, stdout=slave, stderr=slave, stdin=slave)
    os.close(slave)
    out = b""
    while True:
        try:
            d = os.read(master, 4096)
            if not d:
                break
            out += d
        except OSError:
            break
    os.close(master)
    p.wait()

    lines = [l for l in out.split(b"\n") if l.strip(b"\r")]
    grid = []
    for line in lines:
        line = line.strip(b"\r")
        cells = []
        pos = 0
        cur_color = 7
        while pos < len(line):
            m = re.match(rb"^\x1b\[([0-9;]*)m", line[pos:])
            if m:
                raw = m.group(1).decode("ascii", errors="ignore")
                params = [int(x) for x in raw.split(";") if x.isdigit()] if raw else [0]
                cur_color = _parse_sgr(params, cur_color)
                pos += len(m.group(0))
                continue
            m = re.match(rb"^\x1b\[[0-9;?]*[A-Za-z]", line[pos:])
            if m:
                pos += len(m.group(0))
                continue
            if line[pos] == 0x1B:
                pos += 1
                continue
            b0 = line[pos]
            if b0 < 0x80:
                cells.append((chr(b0), cur_color))
                pos += 1
            else:
                n = 2 if b0 < 0xE0 else (3 if b0 < 0xF0 else 4)
                try:
                    ch = line[pos : pos + n].decode("utf-8")
                except UnicodeDecodeError:
                    ch = line[pos : pos + 1].decode("latin1")
                    n = 1
                cells.append((ch, cur_color))
                pos += n
        grid.append(cells)
    return grid


def build_full_frame(grid, clear_screen=False):
    b = bytearray(b"\033[2J\033[H" if clear_screen else b"\033[H")
    cur_c = -1
    for r in grid:
        for ch, col in r:
            if col != cur_c:
                b += f"\033[38;5;{col}m".encode()
                cur_c = col
            b += ch.encode("utf-8")
        b += b"\r\n"
    return bytes(b)


def build_delta_frame(prev_grid, curr_grid):
    b = bytearray()
    cur_c = cursor_r = cursor_c = -1
    blank = (" ", 0)
    num_rows = max(len(prev_grid), len(curr_grid))
    for r_idx in range(num_rows):
        row_prev = prev_grid[r_idx] if r_idx < len(prev_grid) else []
        row_curr = curr_grid[r_idx] if r_idx < len(curr_grid) else []
        num_cols = max(len(row_prev), len(row_curr))
        c_idx = 0
        while c_idx < num_cols:
            prev_cell = row_prev[c_idx] if c_idx < len(row_prev) else blank
            curr_cell = row_curr[c_idx] if c_idx < len(row_curr) else blank
            if prev_cell != curr_cell:
                target_r = r_idx + 1
                target_c = c_idx + 1
                if cursor_r == target_r and cursor_c <= target_c:
                    skip = target_c - cursor_c
                    if skip == 1:
                        b += b"\033[C"
                    elif skip > 1:
                        b += f"\033[{skip}C".encode()
                else:
                    b += f"\033[{target_r};{target_c}H".encode()
                cursor_r, cursor_c = target_r, target_c

                while c_idx < num_cols:
                    prev_cell = row_prev[c_idx] if c_idx < len(row_prev) else blank
                    curr_cell = row_curr[c_idx] if c_idx < len(row_curr) else blank
                    if prev_cell == curr_cell:
                        break
                    ch, col = curr_cell
                    if col != cur_c:
                        b += f"\033[38;5;{col}m".encode()
                        cur_c = col
                    b += ch.encode("utf-8")
                    cursor_c += 1
                    c_idx += 1
            else:
                c_idx += 1
    return bytes(b)


def generate_video(
    pattern="/tmp/frame_*.png",
    cols=80,
    rows=24,
    fps=30,
    output_header="video_data.h",
    braille=False,
    dither=False,
    threshold=60,
):
    files = sorted(glob.glob(pattern))
    if not files:
        raise SystemExit(f"No frames found matching '{pattern}'")
    print(
        f"Processing {len(files)} frames ({'Braille' if braille else 'ASCII'})...",
        file=sys.stderr,
    )

    grids = []
    for idx, path in enumerate(files):
        g = extract_frame_cells(
            path,
            cols=cols,
            rows=rows,
            braille=braille,
            dither=dither,
            threshold=threshold,
        )
        grids.append(g)
        if (idx + 1) % 25 == 0 or idx == len(files) - 1:
            print(f"  Frame {idx + 1}/{len(files)}...", file=sys.stderr)

    max_rows = max(len(g) for g in grids)
    max_cols = max(len(r) for g in grids for r in g) if grids else 0
    blank = (" ", 0)
    for gi, g in enumerate(grids):
        norm = []
        for r in range(max_rows):
            row = g[r] if r < len(g) else []
            norm.append(row + [blank] * (max_cols - len(row)))
        grids[gi] = norm

    encoded_frames = []
    for i in range(len(grids)):
        full = build_full_frame(grids[i], clear_screen=(i == 0))
        if i == 0:
            encoded_frames.append(full)
        else:
            delta = build_delta_frame(grids[i - 1], grids[i])
            encoded_frames.append(delta if len(delta) < len(full) else full)

    payload = b"".join(encoded_frames)
    total_bytes = len(payload)
    offsets, lens, off = [], [], 0
    for f in encoded_frames:
        offsets.append(off)
        lens.append(len(f))
        off += len(f)

    lines = [
        "/* Auto-generated by gen_video.py. Do not edit manually. */",
        "#pragma once",
        "#include <stdint.h>",
        "",
        f"#define NUM_FRAMES {len(encoded_frames)}",
        f"#define VIDEO_FPS {fps}",
        f"#define VIDEO_TOTAL_BYTES {total_bytes}",
        f"#define FRAME_TICKS (13000000 / {fps}) /* APXGPT4 is 13 MHz */",
        "",
        "static const uint32_t frame_offsets[NUM_FRAMES] = {",
        "    " + ", ".join(str(o) for o in offsets),
        "};",
        "",
        "static const uint16_t frame_lens[NUM_FRAMES] = {",
        "    " + ", ".join(str(l) for l in lens),
        "};",
        "",
        f"static const uint8_t video_payload[{total_bytes}] __attribute__((aligned(4))) = {{",
    ]
    for i in range(0, total_bytes, 16):
        chunk = payload[i : i + 16]
        lines.append("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ",")
    lines.append("};")
    lines.append("")

    with open(output_header, "w") as f:
        f.write("\n".join(lines))
    print(
        f"Generated '{output_header}' ({total_bytes} bytes payload, {len(encoded_frames)} frames @ {fps} FPS)",
        file=sys.stderr,
    )
    return encoded_frames


def preview_video(encoded_frames, fps=30):
    sys.stdout.write("\033[?25l\033[2J\033[H")
    sys.stdout.flush()
    frame_interval = 1.0 / fps
    try:
        while True:
            for f in encoded_frames:
                t0 = time.time()
                os.write(sys.stdout.fileno(), f)
                elapsed = time.time() - t0
                if frame_interval > elapsed:
                    time.sleep(frame_interval - elapsed)
    except KeyboardInterrupt:
        sys.stdout.write("\033[?25h\033[0m\r\n")
        sys.stdout.flush()


def main():
    p = argparse.ArgumentParser(
        description="Convert video frames to optimized C header for playback"
    )
    p.add_argument(
        "--pattern", default="/tmp/frame_*.png", help="Glob pattern for frames"
    )
    p.add_argument(
        "--cols", type=int, default=80, help="Terminal column width (default: 80)"
    )
    p.add_argument(
        "--rows", type=int, default=24, help="Terminal row height (default: 24)"
    )
    p.add_argument("--fps", type=int, default=30, help="Video framerate (default: 30)")
    p.add_argument(
        "-o", "--output", default="video_data.h", help="Output C header file"
    )
    p.add_argument(
        "-b", "--braille", action="store_true", help="Use braille characters"
    )
    p.add_argument("--dither", action="store_true", help="Apply dithering for braille")
    p.add_argument(
        "--threshold",
        type=int,
        default=60,
        help="Braille luminance threshold (default: 60)",
    )
    p.add_argument(
        "--preview", action="store_true", help="Preview playback in terminal"
    )
    a = p.parse_args()

    frames = generate_video(
        pattern=a.pattern,
        cols=a.cols,
        rows=a.rows,
        fps=a.fps,
        output_header=a.output,
        braille=a.braille,
        dither=a.dither,
        threshold=a.threshold,
    )
    if a.preview:
        preview_video(frames, fps=a.fps)


if __name__ == "__main__":
    main()
