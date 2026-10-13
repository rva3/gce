use std::str::FromStr;

use anyhow::Context;
use clap_num::maybe_hex;
use num_traits::{Num, Unsigned};

use crate::{GPR, UIM};

#[derive(Debug, Clone, Copy)]
pub enum SubsysOrReg {
    /// id, offset
    Subsys(u8, u16),
    /// reg
    Reg(u8),
}

#[derive(Debug, Clone, Copy)]
pub enum SubsysOrUIM {
    /// id, offset
    Subsys(u8, u16),
    /// id, offset
    UIM(UIM, u16),
}

#[derive(Debug, Clone, Copy)]
pub enum ImmOrUIM {
    Imm(u64),
    UIM(UIM),
}

impl ImmOrUIM {
    /// i don't care about the type, give me something generic
    ///
    /// !! call only in non-MOVE context !!
    pub fn idc(self) -> u32 {
        match self {
            ImmOrUIM::Imm(imm) => imm as u32,
            ImmOrUIM::UIM(uim) => uim as u32,
        }
    }
}

pub trait StrExt {
    /// instruction args
    fn args(&self) -> impl Iterator<Item = &str>;
    /// helper to parse `rN` from string
    fn parse_reg(&self) -> anyhow::Result<u8>;
    /// helper to parse '#N' or `#0xN` from string
    fn parse_imm<T: Unsigned + From<u8>>(&self) -> anyhow::Result<T>
    where
        <T as Num>::FromStrRadixErr: std::fmt::Display;
    // helper to parse UIM or `#0xN`
    fn parse_imm_or_uim(&self) -> anyhow::Result<ImmOrUIM>;
    // remove `[` and `]`
    fn ptr_normalize(&self) -> &str;
}

impl StrExt for str {
    fn args(&self) -> impl Iterator<Item = &str> {
        self.split(',').map(|s| s.trim()).filter(|s| !s.is_empty())
    }

    fn parse_reg(&self) -> anyhow::Result<u8> {
        let reg = if let Some(s) = self.strip_prefix('r') {
            s.parse::<u8>().context("can't parse reg")?
        } else if let Some(s) = self.strip_prefix('p') {
            s.parse::<u8>().context("can't parse reg")? + GPR::P0 as u8
        } else {
            anyhow::bail!("can't parse reg start");
        };

        Ok(reg)
    }

    fn parse_imm<T: Unsigned + From<u8>>(&self) -> anyhow::Result<T>
    where
        <T as Num>::FromStrRadixErr: std::fmt::Display,
    {
        let s = self.strip_prefix('#').context("can't parse imm")?;
        if let Some(c) = s.strip_prefix('\'')
            && s.len() == 3
            && self.is_ascii()
        {
            Ok(c.as_ascii().unwrap()[0].to_u8().into())
        } else {
            Ok(maybe_hex::<T>(s)
                .map_err(anyhow::Error::msg)
                .context("can't parse imm value")?)
        }
    }

    fn parse_imm_or_uim(&self) -> anyhow::Result<ImmOrUIM> {
        if self.starts_with('#') {
            self.parse_imm::<u64>()
                .context("can't parse imm")
                .map(ImmOrUIM::Imm)
        } else {
            UIM::from_str(self)
                .context("can't parse src reg")
                .map(ImmOrUIM::UIM)
        }
    }

    fn ptr_normalize(&self) -> &str {
        self.trim_matches(['[', ']'])
    }
}

pub trait SplitExt {
    // parse `[subsys=N, #0xOFFSET]` or `rN`
    fn parse_subsys_or_reg(&mut self) -> anyhow::Result<SubsysOrReg>;
    // parse `[subsys=N, #0xOFFSET]` or `[rN, #0xOFFSET]`
    fn parse_subsys_or_uim(&mut self) -> anyhow::Result<SubsysOrUIM>;
}

impl<'a, T: Iterator<Item = &'a str>> SplitExt for T {
    fn parse_subsys_or_reg(&mut self) -> anyhow::Result<SubsysOrReg> {
        let s = self
            .next()
            .context("missing subsys or reg")?
            .ptr_normalize()
            .trim();
        if s.starts_with('r') || s.starts_with('p') {
            let reg = s.parse_reg().context("can't parse src reg")?;
            Ok(SubsysOrReg::Reg(reg))
        } else if s.starts_with("subsys") {
            let s = s.strip_prefix("subsys=").unwrap();
            let subsys = s.parse::<u8>().context("can't parse subsys id")?;
            let offset = self
                .next()
                .context("missing offset")?
                .ptr_normalize()
                .parse_imm::<u16>()
                .context("can't parse offset")?;
            Ok(SubsysOrReg::Subsys(subsys, offset))
        } else {
            anyhow::bail!("can't parse instruction");
        }
    }

    fn parse_subsys_or_uim(&mut self) -> anyhow::Result<SubsysOrUIM> {
        let s = self
            .next()
            .context("missing subsys or reg")?
            .ptr_normalize()
            .trim();

        let offset = self
            .next()
            .context("missing offset")?
            .ptr_normalize()
            .parse_imm::<u16>()
            .context("can't parse offset")?;

        if s.starts_with("subsys") {
            let s = s.strip_prefix("subsys=").unwrap();
            let id = s.parse::<u8>().context("can't parse subsys id")?;

            Ok(SubsysOrUIM::Subsys(id, offset))
        } else if s.starts_with(['s', 'c', 'g', 't', 'r'])
        /* FIXME awful parsing */
        {
            let uim = UIM::from_str(s).context("can't parse reg")?;

            Ok(SubsysOrUIM::UIM(uim, offset))
        } else {
            anyhow::bail!("can't parse instruction");
        }
    }
}
