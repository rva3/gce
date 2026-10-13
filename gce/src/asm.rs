use std::{
    collections::HashMap,
    fs,
    path::{Path, PathBuf},
};

use anyhow::Context;
use clap::Parser;
use gce::{Inst, Keyword, UIM};
use std::str::FromStr;

#[derive(Parser)]
struct Cli {
    /// file or raw string
    input: String,
    /// output hex or .bin
    output: Option<PathBuf>,
    /// emit raw binary instead of hex output
    #[arg(short, long)]
    raw: bool,
    /// do not emit addresses in hex mode
    #[arg(short, long)]
    noadddr: bool,
}

fn strip_comment(line: &str) -> &str {
    let bytes = line.as_bytes();
    let mut in_single = false;
    let mut in_double = false;
    let mut i = 0;
    while i < bytes.len() {
        match bytes[i] {
            b'\'' if !in_double => {
                in_single = !in_single;
                i += 1;
            }
            b'"' if !in_single => {
                in_double = !in_double;
                i += 1;
            }
            b'/' if !in_single && !in_double && i + 1 < bytes.len() && bytes[i + 1] == b'/' => {
                return &line[..i];
            }
            _ => i += 1,
        }
    }
    line
}

fn is_label_name(s: &str) -> bool {
    let mut chars = s.chars();
    match chars.next() {
        Some(c) if c.is_ascii_alphabetic() || c == '_' => {}
        _ => return false,
    }
    s.chars()
        .all(|c| c.is_ascii_alphanumeric() || c == '_' || c == '.')
}

fn split_cmd(line: &str) -> (&str, &str) {
    match line.find([' ', '\t']) {
        Some(idx) => (line[..idx].trim(), line[idx..].trim()),
        None => (line.trim(), ""),
    }
}

fn main() -> anyhow::Result<()> {
    let cli = Cli::parse();
    let content = if Path::new(&cli.input).exists() {
        fs::read_to_string(cli.input).context("can't read input file")?
    } else {
        cli.input
    };

    if cli.raw && cli.noadddr {
        anyhow::bail!("raw and noaddr can't be used together");
    } else if cli.raw && cli.output.is_none() {
        anyhow::bail!("raw mode needs output file");
    }

    let args = content
        .split([';', '\n'])
        .map(|arg| strip_comment(arg).trim())
        .filter(|arg| !arg.is_empty())
        .collect::<Vec<_>>();

    let dir = args
        .iter()
        .filter(|arg| arg.starts_with('.'))
        .map(|arg| Keyword::try_from(*arg))
        .collect::<anyhow::Result<Vec<_>>>()?;

    let base_pc = dir
        .iter()
        .find_map(|k| match k {
            Keyword::Base(addr) => Some(*addr),
            _ => None,
        })
        .unwrap_or(0);

    let mut vec = if cli.raw {
        Some(Vec::with_capacity(args.len() * 8))
    } else {
        None
    };

    // this is AWFUL but i dont care

    // pass 1: collect labels with their PCs
    let mut labels: HashMap<String, u32> = HashMap::new();
    let mut program: Vec<(u32, String)> = Vec::new();
    let mut pc = base_pc;
    for line in args.iter().filter(|l| !l.starts_with('.')) {
        let mut rest = line.trim();

        loop {
            let Some(colon) = rest.find(':') else {
                break;
            };
            let (before, after) = rest.split_at(colon);
            let name = before.trim();

            if name.is_empty() || name.contains([' ', '\t', ',', '[', ']']) || !is_label_name(name)
            {
                break;
            }
            if UIM::from_str(name).is_ok() {
                anyhow::bail!("label '{name}' uses register name");
            }
            if labels.insert(name.to_string(), pc).is_some() {
                anyhow::bail!("duplicate label '{name}'");
            }
            rest = after[1..].trim();
            if rest.is_empty() {
                break;
            }

            if !rest.contains(':') {
                break;
            }
        }

        if rest.is_empty() {
            continue;
        }

        program.push((pc, rest.to_string()));
        pc += 8;
    }

    // pass 2: use labels
    for (inst_pc, line) in &program {
        let (cmd, s) = split_cmd(line);
        let resolved: String;
        let s = if cmd == "jump" || cmd == "jmp" {
            let target = s.trim();
            if target.starts_with('#') {
                let inner = target[1..].trim();
                if is_label_name(inner) {
                    let addr = labels
                        .get(inner)
                        .with_context(|| format!("unknown label '{inner}' at {inst_pc:08x}"))?;
                    resolved = format!("#0x{addr:x}");
                    resolved.as_str()
                } else {
                    target
                }
            } else if is_label_name(target) {
                if UIM::from_str(target).is_ok() && !labels.contains_key(target) {
                    target
                } else {
                    let addr = labels
                        .get(target)
                        .with_context(|| format!("unknown label '{target}' at {inst_pc:08x}"))?;
                    let offset = *addr as i64 - *inst_pc as i64;
                    let offset = i32::try_from(offset)
                        .with_context(|| format!("jump to '{target}' out of range"))?;
                    resolved = format!("{offset:+}");
                    resolved.as_str()
                }
            } else {
                target
            }
        } else if matches!(cmd, "jeq" | "jne" | "jge" | "jle" | "jgt" | "jlt") {
            match s.split_once(',') {
                Some((first, tail)) => {
                    let target = first.trim();
                    if is_label_name(target)
                        && !(UIM::from_str(target).is_ok() && !labels.contains_key(target))
                    {
                        let addr = labels.get(target).with_context(|| {
                            format!("unknown label '{target}' at {inst_pc:08x}")
                        })?;
                        let offset = *addr as i64 - *inst_pc as i64;
                        if offset < i16::MIN as i64 || offset > i16::MAX as i64 {
                            anyhow::bail!("conditional jump to '{target}' out of range ({offset})");
                        }
                        resolved = format!("{offset:+},{tail}");
                        resolved.as_str()
                    } else {
                        s
                    }
                }
                None => {
                    let target = s.trim();
                    if is_label_name(target)
                        && !(UIM::from_str(target).is_ok() && !labels.contains_key(target))
                    {
                        let addr = labels.get(target).with_context(|| {
                            format!("unknown label '{target}' at {inst_pc:08x}")
                        })?;
                        let offset = *addr as i64 - *inst_pc as i64;
                        if offset < i16::MIN as i64 || offset > i16::MAX as i64 {
                            anyhow::bail!("conditional jump to '{target}' out of range ({offset})");
                        }
                        resolved = format!("{offset:+}");
                        resolved.as_str()
                    } else {
                        s
                    }
                }
            }
        } else if cmd == "ldi" {
            match s.split_once(',') {
                Some((dst, src)) => {
                    let dst = dst.trim();
                    let src = src.trim();
                    let inner = src.strip_prefix('#').unwrap_or(src).trim();
                    if is_label_name(inner)
                        && !(UIM::from_str(inner).is_ok() && !labels.contains_key(inner))
                    {
                        let addr = labels
                            .get(inner)
                            .with_context(|| format!("unknown label '{inner}' at {inst_pc:08x}"))?;
                        resolved = format!("{dst}, #0x{addr:x}");
                        resolved.as_str()
                    } else {
                        s
                    }
                }
                None => s,
            }
        } else {
            s
        };

        let code = Inst::try_from_cmd_and_args(cmd, s)
            .with_context(|| format!("can't parse '{cmd} {s}' instruction"))?;
        code.verify()
            .with_context(|| format!("invalid instruction '{cmd} {s}'"))?;

        let raw = code.encode().context("failed to encode instruction")?;

        if let Some(v) = &mut vec {
            v.extend(raw.to_le_bytes());
        } else {
            if cli.noadddr {
                println!("{raw:#018x}");
            } else {
                println!("{inst_pc:08x}: {raw:#018x}");
            }
        }
    }

    if let Some(v) = &mut vec {
        fs::write(cli.output.unwrap(), v).context("failed to write into file")?;
    }

    Ok(())
}
