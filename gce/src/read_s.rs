use std::{fmt::Display, str::FromStr};

use anyhow::Context;
use bilge::prelude::*;

use crate::{
    Instruction, UIM,
    asmutils::{SplitExt, StrExt, SubsysOrUIM},
    uim, uim_valid,
};

#[bitsize(64)]
#[derive(TryFromBits, DebugBits, Clone, Copy)]
pub struct Read_S {
    #[at(16)]
    addr_low: u16,
    dst: u16,
    subsys: u5,
    #[at(54)]
    dst_is_reg: u1,
    #[at(56)]
    op: u8,
}

impl Instruction for Read_S {
    fn verify(self) -> anyhow::Result<()> {
        uim!(self.dst()).context("dst reg is out of range")?;

        if self.indirect() {
            let src = uim!(self.subsys().value()).context("src reg is out of range")?;

            anyhow::ensure!(src.is_spr(), "src reg must be spr (5-bit encoding)");
        }
        Ok(())
    }

    fn from_str(_cmd: &str, s: &str) -> anyhow::Result<Self> {
        let mut args = s.args();
        let dst = UIM::from_str(args.next().context("missing dst reg")?)
            .context("can't parse dst reg")?;
        match args
            .parse_subsys_or_uim()
            .context("can't parse subsys or uim")?
        {
            SubsysOrUIM::Subsys(id, offset) => Ok(Self::new(
                offset,
                dst as u16,
                u5::new(id),
                u1::from(true),
                0x80,
            )),
            SubsysOrUIM::UIM(uim, low) => {
                anyhow::ensure!(uim.is_spr(), "reg is out of range. use ldi or write");
                Ok(Self::new(
                    low | 1,
                    dst as u16,
                    u5::new(uim as u8),
                    u1::from(true),
                    0x80,
                ))
            }
        }
    }
}

impl Display for Read_S {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let dst = uim_valid!(self.dst());

        write!(f, "read_s {dst}, ")?;

        if self.indirect() {
            write!(
                f,
                "[{}, #{:#x}]",
                uim_valid!(self.subsys().value()),
                self.unmask()
            )
        } else {
            write!(f, "[subsys={}, #{:#x}]", self.subsys(), self.unmask())
        }
    }
}

impl Read_S {
    fn indirect(&self) -> bool {
        self.addr_low() & 1 != 0
    }

    fn unmask(&self) -> u16 {
        self.addr_low() & !1
    }
}
