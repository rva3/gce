use std::fmt::Display;

use anyhow::Context;
use bilge::prelude::*;

use crate::{
    GPR, Instruction,
    asmutils::{SplitExt, StrExt, SubsysOrReg},
};

#[bitsize(64)]
#[derive(TryFromBits, DebugBits, Clone, Copy)]
pub struct Read {
    dst_idx: u16,
    #[at(32..=47)]
    offset: u16,
    #[at(48..=52)]
    src_idx: u5,
    #[at(53)]
    reserved_c_t: u1,
    #[at(54)]
    data_is_reg: u1,
    #[at(55)]
    addr_is_reg: u1,
    #[at(56)]
    op: u8,
}

impl Instruction for Read {
    fn verify(self) -> anyhow::Result<()> {
        anyhow::ensure!(
            self.dst_idx().value() <= GPR::P7 as u16,
            "dst reg is out of range"
        );

        if !self.is_subsys() {
            anyhow::ensure!(
                self.src_idx().value() <= GPR::P7 as u8,
                "src reg is out of range"
            );
        }

        Ok(())
    }

    fn from_str(_cmd: &str, s: &str) -> anyhow::Result<Self> {
        let mut args = s.args();
        let dst = args
            .next()
            .context("missing reg")?
            .parse_reg()
            .context("can't parse dst reg")?;
        let reg_or_subsys = args
            .parse_subsys_or_reg()
            .context("can't parse subsys or reg")?;
        match reg_or_subsys {
            SubsysOrReg::Subsys(subsys, offset) => Ok(Self::new(
                dst as _,
                offset,
                u5::new(subsys),
                u1::new(1),
                u1::new(0),
                0x01,
            )),
            SubsysOrReg::Reg(src_reg) => Ok(Self::new(
                dst as _,
                0,
                u5::new(src_reg),
                u1::new(1),
                u1::new(1),
                0x01,
            )),
        }
    }
}

impl Read {
    pub fn is_subsys(&self) -> bool {
        self.addr_is_reg().value() == 0
    }
}

impl Display for Read {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if self.is_subsys() {
            write!(
                f,
                "read r{}, [subsys={}, #{:#x}]",
                self.dst_idx(),
                self.src_idx(),
                self.offset()
            )
        } else {
            write!(f, "read r{}, [r{}]", self.dst_idx(), self.src_idx())
        }
    }
}
