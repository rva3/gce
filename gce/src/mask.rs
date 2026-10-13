use std::fmt::Display;

use anyhow::Context;
use bilge::prelude::*;

use crate::{GPR, Instruction, asmutils::StrExt};

#[bitsize(64)]
#[derive(TryFromBits, DebugBits, Clone, Copy)]
pub struct Move {
    arg_b: u32,
    hi: u16,
    idx: u5,
    reserved_c_t: u1,
    reserved_b_t: u1,
    #[at(55)]
    s_op: u1,
    #[at(56)]
    op: u8,
}

impl Instruction for Move {
    fn verify(self) -> anyhow::Result<()> {
        anyhow::ensure!(
            self.is_mask() || self.idx().value() <= GPR::P7 as u8,
            "reg is out of range"
        );

        Ok(())
    }

    fn from_str(cmd: &str, s: &str) -> anyhow::Result<Self> {
        match cmd {
            "mask" => {
                let mask = s.parse_imm::<u32>().context("can't parse mask")?;
                Ok(Self::new(!mask, 0, u5::new(0), u1::new(0), 0x02))
            }
            "move" => {
                let mut args = s.args();
                let idx = args
                    .next()
                    .context("missing register")?
                    .parse_reg()
                    .context("can't parse dst reg")?;
                let imm = args
                    .next()
                    .context("missing imm")?
                    .parse_imm::<u64>()
                    .context("can't parse imm")?;
                Ok(Self::new(
                    imm as u32,
                    (imm >> 32) as u16,
                    u5::new(idx),
                    u1::new(1),
                    0x02,
                ))
            }
            _ => unreachable!(), // already validated in Inst enum
        }
    }
}

impl Move {
    fn is_mask(&self) -> bool {
        self.s_op().value() == 0
    }

    fn imm48(&self) -> u64 {
        self.arg_b() as u64 | (self.hi() as u64) << 32
    }
}

impl Display for Move {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if self.is_mask() {
            write!(f, "mask #{:#x}", !self.arg_b())
        } else {
            write!(f, "move r{}, #{:#x}", self.idx(), self.imm48())
        }
    }
}
