use std::fmt::Display;

use anyhow::Context;
use bilge::prelude::*;

use crate::{
    GPR, Instruction,
    asmutils::{SplitExt, StrExt, SubsysOrReg},
};

#[bitsize(64)]
#[derive(TryFromBits, DebugBits, Clone, Copy)]
pub struct Write {
    value: u32,
    offset: u16,
    subsys: u5,
    #[at(54)]
    value_is_reg: u1,
    #[at(55)]
    addr_is_reg: u1,
    #[at(56)]
    op: u8,
}

impl Instruction for Write {
    fn verify(self) -> anyhow::Result<()> {
        if self.addr_is_reg().value() != 0 {
            // GPR[subsys]
            anyhow::ensure!(
                self.subsys().value() <= GPR::R15 as u8,
                "addr reg is out of range"
            );
        }

        if self.value_is_reg().value() != 0 {
            // GPR[value]
            anyhow::ensure!(self.value() <= GPR::R15 as u32, "data reg is out of range");
        }

        Ok(())
    }

    fn from_str(cmd: &str, s: &str) -> anyhow::Result<Self> {
        let use_mask = cmd == "mwrite";

        let mut args = s.args();
        let reg_or_subsys = args
            .parse_subsys_or_reg()
            .context("failed to parse subsys or dst reg")?;
        let reg_or_imm = args.next().context("missing src reg or imm")?;
        let use_src_reg = reg_or_imm.starts_with('r');
        let reg_or_imm = if use_src_reg {
            reg_or_imm.parse_reg().context("can't parse src reg")? as u32
        } else {
            reg_or_imm.parse_imm::<u32>().context("can't parse imm")?
        };

        let off = if use_mask { 1 } else { 0 };

        match reg_or_subsys {
            SubsysOrReg::Subsys(subsys, offset) => {
                if !use_mask {
                    anyhow::ensure!(
                        offset % 2 == 0,
                        "offset can't have LSB set because it's reserved for mwrite instruction"
                    );
                }
                Ok(Self::new(
                    reg_or_imm,
                    offset | off,
                    u5::new(subsys),
                    u1::from(use_src_reg),
                    u1::new(0),
                    0x04,
                ))
            }
            SubsysOrReg::Reg(dst_reg) => Ok(Self::new(
                reg_or_imm,
                off,
                u5::new(dst_reg),
                u1::from(use_src_reg),
                u1::new(1),
                0x04,
            )),
        }
    }
}

impl Write {
    pub fn is_subsys(&self) -> bool {
        self.addr_is_reg().value() == 0
    }

    pub fn is_using_mask(&self) -> bool {
        self.offset() & 1 != 0
    }

    pub fn unmask(&self) -> u16 {
        self.offset() & !1
    }
}

impl Display for Write {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if self.is_using_mask() {
            write!(f, "mwrite ")?;
        } else {
            write!(f, "write ")?;
        }

        if self.is_subsys() {
            write!(f, "[subsys={}, #{:#x}], ", self.subsys(), self.unmask())?;
        } else {
            write!(f, "[r{}], ", self.subsys())?;
        }

        if self.value_is_reg().value() != 0 {
            write!(f, "r{}", self.value())
        } else {
            write!(f, "#{:#x}", self.value())
        }
    }
}
