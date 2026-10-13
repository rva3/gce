use std::{fmt::Display, str::FromStr};
use strum::{Display, EnumString, FromRepr};

use anyhow::Context;
use bilge::prelude::*;

use crate::{
    Instruction, UIM,
    asmutils::{ImmOrUIM, StrExt},
    uim, uim_valid,
};

#[bitsize(5)]
#[derive(TryFromBits, Debug, Clone, Copy, PartialEq, Eq, Display, EnumString, FromRepr)]
#[strum(serialize_all = "lowercase", ascii_case_insensitive)]
pub enum Operation {
    #[strum(serialize = "ldi")]
    Assign,
    Add,
    Sub,
    Mul,
    Xor = 8,
    Not,
    #[strum(serialize = "orr")]
    Or,
    And,
    Lsl,
    Lsr,
}

#[bitsize(64)]
#[derive(TryFromBits, DebugBits, Clone, Copy)]
pub struct Logic {
    right: u16,
    left: u16,
    dst: u16,
    operation: Operation,
    right_is_reg: u1,
    left_is_reg: u1,
    dst_is_reg: u1,
    op: u8,
}

impl Instruction for Logic {
    fn verify(self) -> anyhow::Result<()> {
        uim!(self.dst()).context("dst reg is out of range")?;

        if self.operation() == Operation::Assign {
            anyhow::ensure!(
                self.left_is_reg().value() == 0 && self.right_is_reg().value() == 0,
                "assign operation accepts only 2x imm16, not regs"
            );
        } else if self.operation() == Operation::Not {
            anyhow::ensure!(self.right() == 0, "not operation accepts only left operand");
        }

        Ok(())
    }

    fn from_str(cmd: &str, s: &str) -> anyhow::Result<Self> {
        let operation = Operation::try_from(cmd).context("can't parse operation")?;
        let mut args = s.args();
        let dst = UIM::from_str(args.next().context("missing reg")?).context("can't parse reg")?;

        let (left_is_reg, left) = match args
            .next()
            .context("missing left operand")?
            .parse_imm_or_uim()
            .context("can't parse left operand")?
        {
            ImmOrUIM::Imm(imm) => (false, imm as u32),
            ImmOrUIM::UIM(uim) => (true, uim as u32),
        };
        if !left_is_reg && operation != Operation::Assign {
            anyhow::ensure!(
                left <= u16::MAX as u32,
                "only imm16 is supported. otherwise use indirect form"
            );
        }

        match operation {
            Operation::Assign => {
                let right = left as u16;
                let left = (left >> 16) as u16;

                anyhow::ensure!(
                    !left_is_reg,
                    "only imm32 is supported. for indirect form use read instruction"
                );

                Ok(Self::new(
                    right,
                    left,
                    dst as u16,
                    operation,
                    u1::from(false),
                    u1::from(false),
                    u1::from(true),
                    0xA0,
                ))
            }
            Operation::Not => Ok(Self::new(
                0,
                left as u16,
                dst as u16,
                operation,
                u1::from(false),
                u1::from(left_is_reg),
                u1::from(true),
                0xA0,
            )),
            _ => {
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

                Ok(Self::new(
                    right as u16,
                    left as u16,
                    dst as u16,
                    operation,
                    u1::from(right_is_reg),
                    u1::from(left_is_reg),
                    u1::from(true),
                    0xA0,
                ))
            }
        }
    }
}

impl Display for Logic {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} {}, ", self.operation(), uim_valid!(self.dst()))?;
        match self.operation() {
            Operation::Assign => write!(f, "#{:#x}", self.imm32()),
            Operation::Not => self.expand_val(f, false),
            _ => {
                self.expand_val(f, false)?;
                write!(f, ", ")?;
                self.expand_val(f, true)
            }
        }
    }
}

impl Logic {
    fn imm32(&self) -> u32 {
        ((self.left() as u32) << 16) | self.right() as u32
    }

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
