use std::env;

use anyhow::Context;
use clap_num::maybe_hex;
use gce::Inst;

fn main() -> anyhow::Result<()> {
    let mut args = env::args();
    args.next().context("bad app name")?;

    for (i, a) in args.enumerate() {
        let word = maybe_hex::<u64>(&a)
            .map_err(anyhow::Error::msg)
            .with_context(|| format!("failed to parse argument {a} as hex"))?;
        let inst = Inst::decode(word).context("can't decode instruction")?;
        inst.verify()
            .with_context(|| format!("can't decode: {inst:?}, raw: {word:#x}"))?;
        println!("{:08x}: {}", i * 8, inst);
    }

    Ok(())
}
