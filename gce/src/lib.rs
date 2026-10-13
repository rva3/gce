#![allow(non_camel_case_types)]
#![feature(ascii_char)]

use std::{
    fmt::{Debug, Display},
    str::FromStr,
};

use crate::{
    asmutils::{ImmOrUIM, StrExt},
    eoc::EOC,
    jump::Jump,
    jump_c::Jump_C,
    logic::Logic,
    mask::Move,
    read::Read,
    read_s::Read_S,
    write::Write,
    write_s::Write_S,
};
use anyhow::Context;
use strum::{Display, EnumString, FromRepr};

mod asmutils;
mod eoc;
mod jump;
mod jump_c;
mod kw;
mod logic;
mod mask;
mod read;
mod read_s;
mod target;
mod write;
mod write_s;

pub use kw::Keyword;

pub trait Instruction: TryFrom<u64> + TryInto<u64> + Display + Copy {
    fn decode(word: u64) -> anyhow::Result<Self>
    where
        <Self as TryFrom<u64>>::Error: Send + Sync + Debug + Display + 'static,
    {
        Self::try_from(word).map_err(anyhow::Error::msg)
    }
    fn encode(self) -> anyhow::Result<u64>
    where
        <Self as TryInto<u64>>::Error: Send + Sync + Debug + Display + 'static,
    {
        self.try_into().map_err(anyhow::Error::msg)
    }
    fn verify(self) -> anyhow::Result<()>;
    fn from_str(cmd: &str, s: &str) -> anyhow::Result<Self>;
}

#[derive(Debug, Copy, Clone)]
pub enum Inst {
    Read(Read),
    Move(Move),
    Write(Write),
    // POLL
    Jump(Jump),
    // WFE/SYNC
    EOC(EOC),
    Read_S(Read_S),
    Write_S(Write_S),
    Logic(Logic),
    Jump_C(Jump_C),
}

impl Display for Inst {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Read(v) => write!(f, "{}", v),
            Self::Move(v) => write!(f, "{}", v),
            Self::Write(v) => write!(f, "{}", v),
            Self::Jump(v) => write!(f, "{}", v),
            Self::EOC(v) => write!(f, "{}", v),
            Self::Read_S(v) => write!(f, "{}", v),
            Self::Write_S(v) => write!(f, "{}", v),
            Self::Logic(v) => write!(f, "{}", v),
            Self::Jump_C(v) => write!(f, "{}", v),
        }
    }
}

impl Inst {
    pub fn decode(word: u64) -> anyhow::Result<Self> {
        let op = (word >> 56) as u64;
        match op {
            0x01 => Ok(Self::Read(Read::decode(word)?)),
            0x02 => Ok(Self::Move(Move::decode(word)?)),
            0x04 => Ok(Self::Write(Write::decode(word)?)),
            0x10 => Ok(Self::Jump(Jump::decode(word)?)),
            0x40 => Ok(Self::EOC(EOC::decode(word)?)),
            0x80 => Ok(Self::Read_S(Read_S::decode(word)?)),
            0x90 | 0x91 => Ok(Self::Write_S(Write_S::decode(word)?)),
            0xA0 => Ok(Self::Logic(Logic::decode(word)?)),
            0xB0 | 0xB1 => Ok(Self::Jump_C(Jump_C::decode(word)?)),

            _ => unreachable!(),
        }
    }

    pub fn encode(&self) -> anyhow::Result<u64> {
        match self {
            Self::Read(v) => v.encode(),
            Self::Move(v) => v.encode(),
            Self::Write(v) => v.encode(),
            Self::Jump(v) => v.encode(),
            Self::EOC(v) => v.encode(),
            Self::Read_S(v) => v.encode(),
            Self::Write_S(v) => v.encode(),
            Self::Logic(v) => v.encode(),
            Self::Jump_C(v) => v.encode(),
        }
    }

    pub fn verify(&self) -> anyhow::Result<()> {
        match self {
            Self::Read(v) => v.verify(),
            Self::Move(v) => v.verify(),
            Self::Write(v) => v.verify(),
            Self::Jump(v) => v.verify(),
            Self::EOC(v) => v.verify(),
            Self::Read_S(v) => v.verify(),
            Self::Write_S(v) => v.verify(),
            Self::Logic(v) => v.verify(),
            Self::Jump_C(v) => v.verify(),
        }
    }

    pub fn try_from_cmd_and_args(cmd: &str, args: &str) -> anyhow::Result<Self> {
        match cmd {
            "read" => Ok(Self::Read(Read::from_str(cmd, args)?)),
            "mask" | "move" => Ok(Self::Move(Move::from_str(cmd, args)?)),
            "mwrite" | "write" => Ok(Self::Write(Write::from_str(cmd, args)?)),
            "jump" | "jmp" => Ok(Self::Jump(Jump::from_str(cmd, args)?)),
            "eoc" | "marker" => Ok(Self::EOC(EOC::from_str(cmd, args)?)),
            "read_s" => Ok(Self::Read_S(Read_S::from_str(cmd, args)?)),
            "mwrite_s" | "write_s" => Ok(Self::Write_S(Write_S::from_str(cmd, args)?)),
            "ldi" | "add" | "sub" | "mul" | "xor" | "not" | "orr" | "and" | "lsl" | "lsr" => {
                Ok(Self::Logic(Logic::from_str(cmd, args)?))
            }
            "jeq" | "jne" | "jge" | "jle" | "jgt" | "jlt" => {
                Ok(Self::Jump_C(Jump_C::from_str(cmd, args)?))
            }

            // pseudo-instructions
            "nop" => Self::try_from_cmd_and_args("jump", "+8").context("failed to emit JUMP +8"),
            /*
             * MOVE when imm <= u48::MAX and dst is GPR
             * LDI when imm <= u32
             * ADD when reg2reg
             */
            "mov" => {
                let mut a = args.args();
                let d = a.next().context("missing dst")?;
                let dst = UIM::from_str(d).context("bad dst")?;
                let s = a.next().context("missing src")?;
                let src = s.parse_imm_or_uim().context("bad src")?;
                match src {
                    ImmOrUIM::Imm(imm) => {
                        if imm > u32::MAX as u64 && imm <= 0xffffffffff && dst.is_gpr() {
                            Self::try_from_cmd_and_args("move", &format!("{d}, #{imm}"))
                                .context("failed to emit MOVE")
                        } else if imm <= u32::MAX as u64 {
                            Self::try_from_cmd_and_args("ldi", &format!("{d}, #{imm}"))
                                .context("failed to emit LDI")
                        } else {
                            anyhow::bail!("no matching encoding the mov instruction");
                        }
                    }
                    ImmOrUIM::UIM(uim) => {
                        Self::try_from_cmd_and_args("add", &format!("{d}, {uim}, #0"))
                            .context("failed to emit empty ADD")
                    }
                }
            }
            _ => anyhow::bail!("unknown instruction"),
        }
    }
}

#[derive(Debug, Clone, Copy)]
#[repr(u8)]
pub enum GPR {
    R0,
    R1,
    R2,
    R3,
    R4,
    R5,
    R6,
    R7,
    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15,

    P0,
    P1,
    P2,
    P3,
    P4,
    P5,
    P6,
    P7,
}

impl Display for GPR {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "R{}", *self as u8)
    }
}

impl GPR {
    pub fn from_u8(v: u8) -> Self {
        assert!(v <= Self::P7 as u8);
        unsafe { std::mem::transmute(v) }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Display, EnumString, FromRepr, PartialOrd, Ord)]
#[strum(serialize_all = "lowercase", ascii_case_insensitive)]
#[repr(u16)]
pub enum UIM {
    #[strum(serialize = "spr0", serialize = "s0")]
    SPR0,
    #[strum(serialize = "spr1", serialize = "s1")]
    SPR1,
    #[strum(serialize = "spr2", serialize = "s2")]
    SPR2,
    #[strum(serialize = "spr3", serialize = "s3")]
    SPR3,

    #[strum(serialize = "gpr0", serialize = "r0")]
    GPR0 = 32,
    #[strum(serialize = "gpr1", serialize = "r1")]
    GPR1,
    #[strum(serialize = "gpr2", serialize = "r2")]
    GPR2,
    #[strum(serialize = "gpr3", serialize = "r3")]
    GPR3,
    #[strum(serialize = "gpr4", serialize = "r4")]
    GPR4,
    #[strum(serialize = "gpr5", serialize = "r5")]
    GPR5,
    #[strum(serialize = "gpr6", serialize = "r6")]
    GPR6,
    #[strum(serialize = "gpr7", serialize = "r7")]
    GPR7,
    #[strum(serialize = "gpr8", serialize = "r8")]
    GPR8,
    #[strum(serialize = "gpr9", serialize = "r9")]
    GPR9,
    #[strum(serialize = "gpr10", serialize = "r10")]
    GPR10,
    #[strum(serialize = "gpr11", serialize = "r11")]
    GPR11,
    #[strum(serialize = "gpr12", serialize = "r12")]
    GPR12,
    #[strum(serialize = "gpr13", serialize = "r13")]
    GPR13,
    #[strum(serialize = "gpr14", serialize = "r14")]
    GPR14,
    #[strum(serialize = "gpr15", serialize = "r15")]
    GPR15,

    TPR = 56,

    HND = 59,

    CPR0T0 = 0x8000,
    CPR1T0 = 0x8001,
    CPR2T0 = 0x8002,
    CPR3T0 = 0x8003,
    CPR0T1 = 0x8004,
    CPR1T1 = 0x8005,
    CPR2T1 = 0x8006,
    CPR3T1 = 0x8007,
    CPR0T2 = 0x8008,
    CPR1T2 = 0x8009,
    CPR2T2 = 0x800a,
    CPR3T2 = 0x800b,
    CPR0T3 = 0x800c,
    CPR1T3 = 0x800d,
    CPR2T3 = 0x800e,
    CPR3T3 = 0x800f,
    CPR0T4 = 0x8010,
    CPR1T4 = 0x8011,
    CPR2T4 = 0x8012,
    CPR3T4 = 0x8013,
    CPR0T5 = 0x8014,
    CPR1T5 = 0x8015,
    CPR2T5 = 0x8016,
    CPR3T5 = 0x8017,
    CPR0T6 = 0x8018,
    CPR1T6 = 0x8019,
    CPR2T6 = 0x801a,
    CPR3T6 = 0x801b,
    CPR0T7 = 0x801c,
    CPR1T7 = 0x801d,
    CPR2T7 = 0x801e,
    CPR3T7 = 0x801f,
    CPR0T8 = 0x8020,
    CPR1T8 = 0x8021,
    CPR2T8 = 0x8022,
    CPR3T8 = 0x8023,
    CPR0T9 = 0x8024,
    CPR1T9 = 0x8025,
    CPR2T9 = 0x8026,
    CPR3T9 = 0x8027,
    CPR0T10 = 0x8028,
    CPR1T10 = 0x8029,
    CPR2T10 = 0x802a,
    CPR3T10 = 0x802b,
    CPR0T11 = 0x802c,
    CPR1T11 = 0x802d,
    CPR2T11 = 0x802e,
    CPR3T11 = 0x802f,
    CPR0T12 = 0x8030,
    CPR1T12 = 0x8031,
    CPR2T12 = 0x8032,
    CPR3T12 = 0x8033,
    CPR0T13 = 0x8034,
    CPR1T13 = 0x8035,
    CPR2T13 = 0x8036,
    CPR3T13 = 0x8037,
    CPR0T14 = 0x8038,
    CPR1T14 = 0x8039,
    CPR2T14 = 0x803a,
    CPR3T14 = 0x803b,
    CPR0T15 = 0x803c,
    CPR1T15 = 0x803d,
    CPR2T15 = 0x803e,
    CPR3T15 = 0x803f,
}

impl UIM {
    pub fn is_spr(&self) -> bool {
        *self >= Self::SPR0 && *self <= Self::SPR3
    }

    pub fn is_gpr(&self) -> bool {
        *self >= Self::GPR0 && *self <= Self::GPR15
    }

    pub fn is_cpr(&self) -> bool {
        *self >= Self::CPR0T0 && *self <= Self::CPR3T15
    }
}

#[macro_export]
macro_rules! uim {
    ($v:expr) => {
        UIM::from_repr($v as _)
    };
}

#[macro_export]
macro_rules! uim_valid {
    ($v:expr) => {
        uim!($v).expect("should've validated")
    };
}
