use std::{
    fs,
    io::{Write, stdout},
    path::PathBuf,
};

use anyhow::{Context, bail};
use clap::Parser;
use clap_num::maybe_hex;
use rapidhash::RapidHashMap;

const UART_BASE: u32 = 0x11002000;
const GPT4_BASE: u32 = 0x10008048;
const GPT4_INC: u32 = 7913;
const CLINT_MTIME: u32 = 0x200BFF8;

#[derive(Parser)]
struct Cli {
    // input
    #[arg(long)]
    input: PathBuf,
    /// buffer base
    #[arg(long, value_parser = maybe_hex::<u32>)]
    gce_base: u32,
    /// riscv binary
    #[arg(long)]
    guest: Option<PathBuf>,
    /// riscv base
    #[arg(long, value_parser = maybe_hex::<u32>)]
    guest_base: u32,
    /// riscv mem size
    #[arg(long, value_parser = maybe_hex::<u32>, default_value = "0x1000000")]
    mem_size: u32,
    /// start PC
    #[arg(long, value_parser = maybe_hex::<u32>)]
    start: Option<u32>,
    /// map steps
    #[arg(long, default_value = "10000000000")]
    limit: u64,
}

struct State {
    gpr: [u32; 16],
    spr: [u32; 4],
    cpr: [u32; 64],
    tpr: u32,
    hnd: u32,
    mask: u32,
    gpt4: u32,
    pc: u32,
    steps: u64,
    uart: Vec<u8>,
    gce: Vec<u64>,
    gce_base: u32,
    guest: Vec<u32>,
    guest_base: u32,
    extra: RapidHashMap<u32, u32>,
}

impl State {
    #[inline(always)]
    fn uim_get(&self, uim: u32) -> Result<u32, anyhow::Error> {
        match uim {
            0..=3 => Ok(self.spr[uim as usize]),
            32..=47 => Ok(self.gpr[(uim - 32) as usize]),
            56 => Ok(self.tpr),
            59 => Ok(self.hnd),
            0x8000..=0x803F => Ok(self.cpr[(uim - 0x8000) as usize]),
            _ => bail!("bad UIM read {uim:#x} at pc {:#x}", self.pc),
        }
    }

    #[inline(always)]
    fn uim_set(&mut self, uim: u32, val: u32) -> Result<(), anyhow::Error> {
        match uim {
            0..=3 => self.spr[uim as usize] = val,
            32..=47 => self.gpr[(uim - 32) as usize] = val,
            56 => self.tpr = val,
            59 => self.hnd = val,
            0x8000..=0x803F => self.cpr[(uim - 0x8000) as usize] = val,
            _ => bail!("bad UIM write {uim:#x} at pc {:#x}", self.pc),
        }
        Ok(())
    }

    #[inline(always)]
    fn mem_read(&mut self, addr: u32) -> Result<u32, anyhow::Error> {
        anyhow::ensure!(addr & 3 == 0, "misaligned read");
        match addr {
            a if a == UART_BASE => Ok(0),
            a if a == UART_BASE + 4 => Ok(0x6000),
            a if a == UART_BASE + 0x14 => Ok(0x60),
            a if a == GPT4_BASE || a == CLINT_MTIME => {
                self.gpt4 = self.gpt4.wrapping_add(GPT4_INC);
                Ok(self.gpt4)
            }
            _ => {
                if let Some(v) = guest_get(&self.guest, self.guest_base, addr) {
                    Ok(v)
                } else {
                    Ok(self.extra.get(&addr).copied().unwrap_or(0))
                }
            }
        }
    }

    #[inline(always)]
    fn mem_write(&mut self, addr: u32, val: u32) -> Result<(), anyhow::Error> {
        anyhow::ensure!(addr & 3 == 0, "misaligned write");
        if addr == UART_BASE {
            self.uart.push((val & 0xFF) as u8);
            return Ok(());
        }
        let off = addr.wrapping_sub(self.guest_base);
        if off < (self.guest.len() as u32) * 4 {
            self.guest[(off >> 2) as usize] = val;
            return Ok(());
        }
        self.extra.insert(addr, val);
        Ok(())
    }

    #[inline(always)]
    fn fetch(&self) -> u64 {
        let off = self.pc.wrapping_sub(self.gce_base);
        if off & 7 == 0 && (off >> 3) < self.gce.len() as u32 {
            self.gce[(off >> 3) as usize]
        } else {
            0
        }
    }

    fn step(&mut self) -> Result<bool, anyhow::Error> {
        let pc = self.pc;
        let word = self.fetch();
        let op = (word >> 56) as u8;
        let nxt = pc.wrapping_add(8);
        self.steps += 1;

        match op {
            0x01 => {
                let dst = (word & 0xFFFF) as u32;
                let src_idx = ((word >> 48) & 0x1F) as u32;
                let addr_is_reg = ((word >> 55) & 1) != 0;
                if dst > 23 {
                    bail!("READ bad dst {dst} at {pc:#x}");
                }
                if !addr_is_reg {
                    bail!("READ subsys mode not supported at {pc:#x}");
                }
                if src_idx > 23 || src_idx >= 16 || dst >= 16 {
                    bail!("READ P-reg not supported at {pc:#x}");
                }
                let addr = self.gpr[src_idx as usize];
                let v = self.mem_read(addr)?;
                self.gpr[dst as usize] = v;
                self.pc = nxt;
            }
            0x02 => {
                let arg_b = (word & 0xFFFFFFFF) as u32;
                let _hi = ((word >> 32) & 0xFFFF) as u32;
                let idx = ((word >> 48) & 0x1F) as u32;
                let s_op = ((word >> 55) & 1) != 0;
                if !s_op {
                    self.mask = arg_b;
                } else {
                    if idx > 23 || idx >= 16 {
                        bail!("MOVE P-reg not supported at {pc:#x}");
                    }
                    self.gpr[idx as usize] = arg_b;
                    // uh uh uh maybe we should shit into r1 here like hw, but we don't use any P regs currently
                }
                self.pc = nxt;
            }
            0x04 => {
                let value = (word & 0xFFFFFFFF) as u32;
                let offset = ((word >> 32) & 0xFFFF) as u32;
                let subsys = ((word >> 48) & 0x1F) as u32;
                let value_is_reg = ((word >> 54) & 1) != 0;
                let addr_is_reg = ((word >> 55) & 1) != 0;
                if !addr_is_reg {
                    bail!("WRITE subsys mode not supported at {pc:#x}");
                }
                if subsys > 15 || offset > 1 {
                    bail!("WRITE bad addr at {pc:#x}");
                }
                let addr = self.gpr[subsys as usize];
                let mut data = if value_is_reg {
                    if value > 15 {
                        bail!("WRITE bad data reg {value} at {pc:#x}");
                    }
                    self.gpr[value as usize]
                } else {
                    value
                };
                if offset & 1 != 0 {
                    let cur = if addr == UART_BASE {
                        0
                    } else {
                        self.mem_read(addr)?
                    };
                    data = (cur & self.mask) | (data & !self.mask);
                }
                self.mem_write(addr, data)?;
                self.pc = nxt;
            }
            0x10 => {
                let target = (word & 0xFFFFFFFF) as u32;
                let is_abs = ((word >> 32) & 1) != 0;
                let use_reg = ((word >> 54) & 1) != 0;
                if use_reg {
                    bail!("JUMP by_reg broken, use JUMP_C at {pc:#x}");
                }
                self.pc = if is_abs {
                    target
                } else {
                    pc.wrapping_add(target as i32 as u32)
                };
            }
            0x40 => {
                if word & 1 != 0 {
                    return Ok(true);
                }
                self.pc = nxt;
            }
            0x80 => {
                let addr_low = ((word >> 16) & 0xFFFF) as u32;
                let dst = ((word >> 32) & 0xFFFF) as u32;
                let subsys = ((word >> 48) & 0x1F) as u32;
                if addr_low & 1 == 0 {
                    bail!("READ_S subsys mode not supported at {pc:#x}");
                }
                if subsys > 3 {
                    bail!("READ_S indirect bad SPR {subsys} at {pc:#x}");
                }
                let low = addr_low & !1;
                if low & 3 != 0 {
                    bail!("READ_S misaligned low {low:#x} at {pc:#x}");
                }
                let addr = (self.spr[subsys as usize] << 16) | low;
                let v = self.mem_read(addr)?;
                self.uim_set(dst, v)?;
                self.pc = nxt;
            }
            0x90 | 0x91 => {
                let src = (word & 0xFFFFFFFF) as u32;
                let addr_low = ((word >> 32) & 0xFFFF) as u32;
                let subsys = ((word >> 48) & 0x1F) as u32;
                let src_is_reg = ((word >> 54) & 1) != 0;
                if addr_low & 1 == 0 {
                    bail!("WRITE_S subsys mode not supported at {pc:#x}");
                }
                if subsys > 3 {
                    bail!("WRITE_S indirect bad SPR {subsys} at {pc:#x}");
                }
                let low = addr_low & !1;
                if low & 3 != 0 {
                    bail!("WRITE_S misaligned low {low:#x} at {pc:#x}");
                }
                let addr = (self.spr[subsys as usize] << 16) | low;
                let mut data = if src_is_reg {
                    self.uim_get(src & 0xFFFF)?
                } else {
                    src
                };
                if op == 0x91 {
                    let cur = if addr == UART_BASE {
                        0
                    } else {
                        self.mem_read(addr)?
                    };
                    data = (cur & self.mask) | (data & !self.mask);
                }
                self.mem_write(addr, data)?;
                self.pc = nxt;
            }
            0xA0 => {
                let right = (word & 0xFFFF) as u32;
                let left = ((word >> 16) & 0xFFFF) as u32;
                let dst = ((word >> 32) & 0xFFFF) as u32;
                let operation = ((word >> 48) & 0x1F) as u32;
                let right_is_reg = ((word >> 53) & 1) != 0;
                let left_is_reg = ((word >> 54) & 1) != 0;
                match operation {
                    0 => {
                        if left_is_reg || right_is_reg {
                            bail!("LDI with reg flag at {pc:#x}");
                        }
                        self.uim_set(dst, (left << 16) | right)?;
                    }
                    1 | 2 | 3 | 8 | 10 | 11 | 12 | 13 => {
                        let b = if left_is_reg {
                            self.uim_get(left)?
                        } else {
                            left
                        };
                        let c = if right_is_reg {
                            self.uim_get(right)?
                        } else {
                            right
                        };
                        let v = match operation {
                            1 => b.wrapping_add(c),
                            2 => b.wrapping_sub(c),
                            3 => b.wrapping_mul(c),
                            8 => b ^ c,
                            10 => b | c,
                            11 => b & c,
                            12 => {
                                if c >= 32 {
                                    0
                                } else {
                                    b.wrapping_shl(c)
                                }
                            }
                            _ => {
                                if c >= 32 {
                                    0
                                } else {
                                    b.wrapping_shr(c)
                                }
                            }
                        };
                        self.uim_set(dst, v)?;
                    }
                    9 => {
                        let b = if left_is_reg {
                            self.uim_get(left)?
                        } else {
                            left
                        };
                        self.uim_set(dst, !b)?;
                    }
                    _ => bail!("LOGIC bad op {operation} at {pc:#x}"),
                }
                self.pc = nxt;
            }
            0xB0 | 0xB1 => {
                let right = (word & 0xFFFF) as u32;
                let left = ((word >> 16) & 0xFFFF) as u32;
                let target = ((word >> 32) & 0xFFFF) as u32;
                let cond = ((word >> 48) & 0x1F) as u32;
                let right_is_reg = ((word >> 53) & 1) != 0;
                let left_is_reg = ((word >> 54) & 1) != 0;
                let target_is_reg = ((word >> 55) & 1) != 0;
                let l = if left_is_reg {
                    self.uim_get(left)?
                } else {
                    left
                };
                let r = if right_is_reg {
                    self.uim_get(right)?
                } else {
                    right
                };
                let take = match cond {
                    0 => l == r,
                    1 => l != r,
                    2 => l >= r,
                    3 => l <= r,
                    4 => l > r,
                    5 => l < r,
                    _ => bail!("JUMP_C bad cond {cond} at {pc:#x}"),
                };
                if take {
                    if target_is_reg {
                        if op != 0xB0 {
                            bail!("JUMP_C abs with B1 at {pc:#x}");
                        }
                        self.pc = self.uim_get(target)?;
                    } else {
                        if op != 0xB1 {
                            bail!("JUMP_C rel with B0 at {pc:#x}");
                        }
                        self.pc = pc.wrapping_add(target as u16 as i16 as i32 as u32);
                    }
                } else {
                    self.pc = nxt;
                }
            }
            _ => bail!("bad opcode {op:#x} ({word:#018x}) at pc {pc:#x}"),
        }

        Ok(false)
    }
}

#[inline]
fn guest_get(guest: &[u32], base: u32, addr: u32) -> Option<u32> {
    let off = addr.wrapping_sub(base);
    if off < (guest.len() as u32) * 4 {
        Some(guest[(off >> 2) as usize])
    } else {
        None
    }
}

fn uart(state: &mut State) {
    let mut lock = stdout();
    lock.write_all(&state.uart).expect("can't write uart");
    lock.flush().unwrap();
    state.uart.clear();
}

fn main() -> anyhow::Result<()> {
    let cli = Cli::parse();
    let gce = fs::read(cli.input)
        .context("can't read input")?
        .chunks(8)
        .map(|c| u64::from_le_bytes([c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7]]))
        .collect::<Vec<u64>>();

    let words = (cli.mem_size / 4) as usize;
    let mut guest = vec![0u32; words];
    if let Some(p) = &cli.guest {
        let mut data = fs::read(p).context("can't read riscv bin")?;
        while data.len() % 4 != 0 {
            data.push(0);
        }
        anyhow::ensure!(data.len() / 4 <= words, "guest bigger than mem size");
        for (i, chunk) in data.chunks(4).enumerate() {
            guest[i] = u32::from_le_bytes([chunk[0], chunk[1], chunk[2], chunk[3]]);
        }
        eprintln!("guest {} bytes at {:#x}", data.len(), cli.guest_base);
    }

    let mut state = State {
        gpr: [0; 16],
        spr: [0; 4],
        cpr: [0; 64],
        tpr: 0,
        hnd: 0,
        mask: 0,
        gpt4: 0,
        pc: cli.start.unwrap_or(cli.gce_base),
        steps: 0,
        uart: Vec::with_capacity(1 << 20),
        gce,
        gce_base: cli.gce_base,
        guest,
        guest_base: cli.guest_base,
        extra: RapidHashMap::default(),
    };

    eprintln!(
        "gce {} insns base {:#x} start {:#x}",
        state.gce.len(),
        cli.gce_base,
        state.pc
    );
    let mut halted = false;
    let mut post_left: Option<u64> = None;
    while state.steps < cli.limit {
        match state.step() {
            Ok(done) => {
                if done {
                    halted = true;
                    break;
                }
            }
            Err(e) => {
                dumpregs(&state, Some(&e));
                uart(&mut state);
                bail!("error after {} steps: {e:#}", state.steps);
            }
        }
        if state.steps & 0xFFFFF == 0 {
            uart(&mut state);
        }
        if let Some(n) = post_left {
            if n == 0 {
                break;
            }
            post_left = Some(n - 1);
        }
    }

    eprintln!(
        "steps={} halted={} pc={:#x} r15={:#x} uart={} bytes",
        state.steps,
        halted,
        state.pc,
        state.gpr[15],
        state.uart.len()
    );

    uart(&mut state);

    Ok(())
}

fn dumpregs(st: &State, err: Option<&anyhow::Error>) {
    if let Some(e) = err {
        eprintln!(
            "ERR steps={} pc={:#x} r15={:#x} INSN={:#x} err={e:#}",
            st.steps, st.pc, st.gpr[15], st.gpr[14]
        );
    }
    eprintln!(
        "gpr={:08x?} spr={:08x?} tpr={:#x} mask={:#x}",
        st.gpr, st.spr, st.tpr, st.mask
    );
    for (tid, chunk) in st.cpr.chunks(4).enumerate() {
        eprintln!(
            "cpr-t{tid:2}: {:08x?}",
            [chunk[0], chunk[1], chunk[2], chunk[3]]
        );
    }
}
