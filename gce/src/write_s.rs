use std::fmt::Display;

use anyhow::Context;
use bilge::prelude::*;

use crate::{
    Instruction, UIM,
    asmutils::{SplitExt, StrExt, SubsysOrUIM},
    uim, uim_valid,
};

#[bitsize(64)]
#[derive(TryFromBits, DebugBits, Clone, Copy)]
pub struct Write_S {
    src: u32,
    addr_low: u16,
    subsys: u5,
    #[at(54)]
    src_is_reg: u1,
    #[at(56)]
    op: u8,
}

impl Instruction for Write_S {
    fn verify(self) -> anyhow::Result<()> {
        if self.indirect() {
            let high = uim!(self.subsys().value()).context("addr HIGH reg is out of range")?;
            anyhow::ensure!(high.is_spr(), "addr HIGH must be SPR (5-bit s_op encoding)");
        }

        if self.src_is_reg().value() != 0 {
            uim!(self.src()).context("src reg is out of range")?;
        }

        anyhow::ensure!(self.unmask() % 4 == 0, "addr must be 4-byte aligned");

        Ok(())
    }

    fn from_str(cmd: &str, s: &str) -> anyhow::Result<Self> {
        let use_mask = cmd == "mwrite_s";
        let mut args = s.args();
        let dst = args
            .parse_subsys_or_uim()
            .context("can't parse subsys or uim")?;
        let s = args.next().context("missing src")?;
        let src_is_reg = !s.starts_with('#');
        let src = s
            .parse_imm_or_uim()
            .context("can't parse imm or uim")?
            .idc();
        let op = if use_mask { 0x91 } else { 0x90 };

        match dst {
            SubsysOrUIM::Subsys(id, offset) => Ok(Self::new(
                src,
                offset,
                u5::new(id),
                u1::from(src_is_reg),
                op,
            )),
            SubsysOrUIM::UIM(uim, low) => {
                anyhow::ensure!(uim.is_spr(), "reg is out of range. use ldi or write");
                Ok(Self::new(
                    src,
                    low | 1,
                    u5::new(uim as u8),
                    u1::from(src_is_reg),
                    op,
                ))
            }
        }
    }
}

impl Display for Write_S {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if self.op() == 0x90 {
            write!(f, "write_s ")?
        } else {
            write!(f, "mwrite_s ")?
        };
        if self.indirect() {
            write!(
                f,
                "[{}, #{:#x}], ",
                uim_valid!(self.subsys().value()),
                self.unmask()
            )?;
        } else {
            write!(f, "[subsys={}, #{:#x}], ", self.subsys(), self.unmask())?;
        }

        if self.src_is_reg().value() != 0 {
            write!(f, "{}", uim_valid!(self.src()))
        } else {
            write!(f, "#{:#x}", self.src())
        }
    }
}

impl Write_S {
    fn indirect(&self) -> bool {
        self.addr_low() & 1 != 0
    }

    fn unmask(&self) -> u16 {
        self.addr_low() & !1
    }
}
