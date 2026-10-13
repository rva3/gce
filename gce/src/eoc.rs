use std::fmt::Display;

use bilge::prelude::*;

use crate::Instruction;

#[bitsize(64)]
#[derive(TryFromBits, DebugBits, Clone, Copy)]
pub struct EOC {
    irq: u1,
    #[at(16)]
    prefetch_en: u1,
    #[at(17)]
    prefetch_en2: u1,
    #[at(20)]
    prefetch_mark: u1,
    #[at(48)]
    no_inc: u1,
    #[at(53)]
    no_suspend: u1,
    #[at(56)]
    op: u8,
}

impl Instruction for EOC {
    fn verify(self) -> anyhow::Result<()> {
        if self.irq().value() != 0 {
            anyhow::ensure!(
                self.prefetch_en().value() == 0
                    && self.prefetch_en2().value() == 0
                    && self.prefetch_mark().value() == 0
                    && self.no_inc().value() == 0
                    && self.no_suspend().value() == 0,
                "eoc shouldn't set any flags"
            );
        }

        if self.prefetch_en().value() != 0 || self.prefetch_en2().value() != 0 {
            anyhow::ensure!(
                self.prefetch_mark().value() != 0,
                "prefetch can't be enabled without prefetch marker"
            );

            anyhow::ensure!(
                self.prefetch_en().value() != 0 && self.prefetch_en2().value() != 0,
                "both prefetch enable flags should be set"
            );
        }

        Ok(())
    }

    fn from_str(cmd: &str, s: &str) -> anyhow::Result<Self> {
        match cmd {
            "eoc" => Ok(Self::new(
                u1::from(true),
                u1::from(false),
                u1::from(false),
                u1::from(false),
                u1::from(false),
                u1::from(false),
                0x40,
            )),
            "marker" => {
                let prefetch = s.contains("prefetch");
                let no_inc = !s.contains("inc");
                let no_suspend = !s.contains("suspend");

                Ok(Self::new(
                    u1::from(false),
                    u1::from(prefetch),
                    u1::from(prefetch),
                    u1::from(prefetch),
                    u1::from(no_inc),
                    u1::from(no_suspend),
                    0x40,
                ))
            }
            _ => unreachable!(), // already validated in Inst enum
        }
    }
}

impl Display for EOC {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if self.irq().value() != 0 {
            write!(f, "eoc")
        } else {
            write!(f, "marker")?;

            let mut sep = " ";

            if self.prefetch_en().value() != 0 {
                write!(f, "{sep}prefetch")?;
                sep = ", ";
            }

            if self.no_inc().value() == 0 {
                write!(f, "{sep}inc")?;
                sep = ", ";
            }

            if self.no_suspend().value() == 0 {
                write!(f, "{sep}suspend")?;
            }

            Ok(())
        }
    }
}
