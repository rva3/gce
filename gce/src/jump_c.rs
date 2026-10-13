use std::{fmt::Display, str::FromStr};
use strum::{Display, EnumString, FromRepr};

use anyhow::Context;
use bilge::prelude::*;

use crate::{
    Instruction, UIM,
    asmutils::{ImmOrUIM, StrExt},
    target::PC_JUMP_SHIFT,
    uim, uim_valid,
};

#[bitsize(5)]
#[derive(TryFromBits, Debug, Clone, Copy, PartialEq, Eq, Display, EnumString, FromRepr)]
#[strum(serialize_all = "lowercase", ascii_case_insensitive)]
pub enum Condition {
    #[strum(serialize = "jeq")]
    Eq,
    #[strum(serialize = "jne")]
    Ne,
    #[strum(serialize = "jge")]
    Ge,
    #[strum(serialize = "jle")]
    Le,
    #[strum(serialize = "jgt")]
    Gt,
    #[strum(serialize = "jlt")]
    Lt,
}

#[bitsize(64)]
#[derive(TryFromBits, DebugBits, Clone, Copy)]
pub struct Jump_C {
    right: u16,
    left: u16,
    target: u16,
    cond: Condition,
    right_is_reg: u1,
    left_is_reg: u1,
    target_is_reg: u1,
    op: u8,
}

impl Instruction for Jump_C {
    fn verify(self) -> anyhow::Result<()> {
        if self.op() == 0xB1 {
            anyhow::ensure!(
                self.target_is_reg().value() == 0,
                "relative jump can't use indirect mode"
            );
        } else {
            anyhow::ensure!(
                self.target_is_reg().value() == 1,
                "absolute jump can't use imm mode"
            );
        }

        if self.target_is_reg().value() != 0 {
            uim!(self.target()).context("target reg is out of range")?;
        }

        if self.left_is_reg().value() != 0 {
            uim!(self.left()).context("left reg is out of range")?;
        }

        if self.right_is_reg().value() != 0 {
            uim!(self.right()).context("right reg is out of range")?;
        }

        Ok(())
    }

    fn from_str(cmd: &str, s: &str) -> anyhow::Result<Self> {
        let cond = Condition::try_from(cmd).context("can't parse operation")?;
        let mut args = s.args();

        let s = args.next().context("missing target")?;
        let (target_is_reg, target) = if let Ok(target) = s.parse::<i32>() {
            (false, target as u32)
        } else {
            (
                true,
                UIM::from_str(s).context("can't parse target reg or bad target")? as u32,
            )
        };

        let (left_is_reg, left) = match args
            .next()
            .context("missing left operand")?
            .parse_imm_or_uim()
            .context("can't parse left operand")?
        {
            ImmOrUIM::Imm(imm) => (false, imm as u32),
            ImmOrUIM::UIM(uim) => (true, uim as u32),
        };
        if !left_is_reg {
            anyhow::ensure!(
                left <= u16::MAX as u32,
                "only imm16 is supported. otherwise use indirect form"
            );
        }

        let (right_is_reg, right) = match args
            .next()
            .context("missing right operand")?
            .parse_imm_or_uim()
            .context("can't parse right operand")?
        {
            ImmOrUIM::Imm(imm) => (false, imm as u32),
            ImmOrUIM::UIM(uim) => (true, uim as u32),
        };
        if !right_is_reg {
            anyhow::ensure!(
                right <= u16::MAX as u32,
                "only imm16 is supported. otherwise use indirect form"
            );
        }

        let op = if target_is_reg { 0xB0 } else { 0xB1 };

        Ok(Self::new(
            right as u16,
            left as u16,
            target as u16,
            cond,
            u1::from(right_is_reg),
            u1::from(left_is_reg),
            u1::from(target_is_reg),
            op,
        ))
    }
}

impl Display for Jump_C {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} ", self.cond())?;
        if self.op() == 0xB1 {
            write!(f, "{:+}", (self.target() as i16 as i32) << PC_JUMP_SHIFT)
        } else {
            write!(f, "{}, ", uim_valid!(self.target()))?;
            self.expand_val(f, false)?;
            write!(f, ", ")?;
            self.expand_val(f, true)
        }
    }
}

impl Jump_C {
    fn expand_val(&self, f: &mut std::fmt::Formatter<'_>, right: bool) -> std::fmt::Result {
        if right {
            if self.right_is_reg().value() != 0 {
                write!(f, "{}", uim_valid!(self.right()))
            } else {
                write!(f, "#{:#x}", self.right())
            }
        } else {
            if self.left_is_reg().value() != 0 {
                write!(f, "{}", uim_valid!(self.left()))
            } else {
                write!(f, "#{:#x}", self.left())
            }
        }
    }
}
