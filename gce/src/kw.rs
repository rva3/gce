use anyhow::Context;
use clap_num::maybe_hex;

#[derive(Debug, Clone)]
pub enum Keyword<'k> {
    Ascii(&'k str),
    Base(u32),
}

impl<'k> TryFrom<&'k str> for Keyword<'k> {
    type Error = anyhow::Error;

    fn try_from(value: &'k str) -> Result<Self, Self::Error> {
        let (cmd, arg) = value.split_once(' ').context("bad directive")?;

        match cmd {
            ".ascii" => {
                let val = arg
                    .strip_prefix('"')
                    .and_then(|s| s.strip_suffix('"'))
                    .context("ascii directive requires quoted string")?;
                Ok(Self::Ascii(val))
            }
            ".base" => Ok(Self::Base(
                maybe_hex::<u32>(arg).map_err(anyhow::Error::msg)?,
            )),
            _ => anyhow::bail!("unknown directive: {cmd}"),
        }
    }
}
