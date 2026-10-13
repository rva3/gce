use std::fmt::Display;

use anyhow::Context;
use bilge::prelude::*;

use crate::{GPR, Instruction, asmutils::StrExt, target::PC_JUMP_SHIFT};

#[bitsize(64)]
#[derive(TryFromBits, DebugBits, Clone, Copy)]
pub struct Jump {
    target: u32,
    is_abs: u1,
    #[at(54)]
    use_reg: u1,
    #[at(56)]
    op: u8,
}

impl Instruction for Jump {
    fn verify(self) -> anyhow::Result<()> {
        if self.use_reg().value() != 0 {
            anyhow::ensure!(
                self.target().value() <= GPR::P7 as u32,
                "reg is out of range"
            );
        }

        Ok(())
    }

    fn from_str(_cmd: &str, s: &str) -> anyhow::Result<Self> {
        let abs = s.starts_with('#');
        let reg = s.starts_with('r') || s.starts_with('p');
        let target = if reg {
            anyhow::bail!("reg jump is BROKEN. use jeq r0, r0 instead.")
        } else if abs {
            s.parse_imm::<u32>().context("can't parse imm")? >> PC_JUMP_SHIFT
        } else {
            s.parse::<i32>().context("can't parse offset")? as u32
        };

        Ok(Self::new(target, u1::from(abs), u1::from(reg), 0x10))
    }
}

impl Display for Jump {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "jump ")?;
        if self.use_reg().value() != 0 {
            write!(f, "r{}", self.target())
        } else if self.is_abs().value() != 0 {
            write!(f, "#{:#x}", self.target() >> PC_JUMP_SHIFT)
        } else {
            write!(f, "{:+}", self.target() as i32)
        }
    }
}
