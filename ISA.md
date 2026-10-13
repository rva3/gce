## Generations
There's multiple GCE generations: legacy, v2 and v3.

It's important to distinguish GCE generations before we go deeper.
- Legacy GCE doesn't have explicit registers, although it looks like they have
something internally. It features only few basic commands and can do only subsys
writes. SoCs using this generation are fragmented and analysis of android blobs
reveals some instruction sequences with unusual usage. They're likely used to
mitigate hardware bugs or poorly managed buffer allocations. Used in mt65xx,
besides mt6577 (no GCE) and 6595 (v2).

- v2 appears to be more advanced, featuring more subsys writes and (likely) full
PA writes. It supports more commands, but it's still a dumb display offloading
engine. It's not clear which SoCs use this generation, but based on various
downstream sources it's mt816x, specifically mt8167 and 8168.

- v3 has logic and conditional jump commands, making it first turing-complete
generation. It's used in at least mt8183 and 6768.

- There are v4 mentions, but I can't tell if it's actual generation or minor
change.

## v3
We will focus on v3 ISA, because it's the only interesting generation.

### Registers
GCE v3 features 16 general purpose registers (GPRs), 4 special purpose registers
(SPRs) and 4 32-bit SRAM blocks (CPRs). There are special registers like timer
(TPR) and handshake (HND). All registers are 32-bit.
- GPRs are aliased as **P0-P7** for 64-bit or **R0-R15** for 32-bit. Another R0
name is **CNT**. They're are global and shared for all threads. They can be
read from the MMIO using `gce_base + 0x80 + 4 * idx`.
- SPRs and CPRs are per-thread. SPRs can be read from `gce_thread_base + 0x160 +
4 * idx`, where `gce_thread_base` is `gce_base + 0x100 + 0x80 * idx`.

For moving data between GPRs and SPRs we can use LOGIC command. All NEW commands
(read below) utilize some register space which allows to access SPRs, CPRs, GPRs
and few special registers.

Unified (NEW instructions) map:
| reg | id    |
| :-: | :---: |
| SPR | 0-3   |
| CPR | 4-7   |
| GPR | 32-47 |
| TPR | 56    |
| HND | 59    |

Let's say you want to move data between GPR and SPR. Assume R0 is 1337. To move
data, use no-op ADD: `ADD SPR0, GPR0, #0`. This becomes `ADD 0, 32, #0` by the
mapping table above.

#### CPR madness
You think this is easy, right? Well, don't worry, it's mediatek, they never made
sane coprocessors. Remember that CPRs are SRAM? To load data into CPR, we need
to use special encoding. `ADD CPR0, SPR0, #0` becomes `ADD 0x8000, 32, #0`.

This code won't work properly if you're using other thread than 0, because CPRs
are accessed using `0x8000 + thread_id * idx`. Note that `idx` means number
after CPR, or `4-idx` if we use linear mapping where CPR starts at 4.

A more complex example: move data between R7 and CPR3 at thread 8.
`ADD CPR3, R7, #0` ->
`ADD 0x8000 (base) + 8 (thread ID) * 4 + 3 (CPR index), 39, #0` ->
`ADD 0x8023, 39, #0`

### Instruction encoding
Instructions are always 64-bit. They're encoded with
<u8 opcode><u24 arg a><u32 arg b>.

Argument A and B almost always have additional encoding depending on the
instruction.

### Write types
There are 2 write types used by the GCE.
- subsystem write is used for display-related tasks. Upstream linux driver
mentions that it's faster than PA write. In legacy there's only 3 subsystem IDs
(2 bit field) and 22-bit offset. Since v2, the offset is 16-bit and subsystem ID
was expanded to 8 bits. Subsystems are fixed in the hardware and usually limited
to display/MDP/VENC/VDEC regions.

- PA write is used for 32-bit value to 32-bit address. Due to ISA limitations,
it's not possible to perform store operation into MMIO address. PA write uses
SPR to store target address.

## ISA
There's at least 2 types of commands. There's no official names, but we will
call them OLD and NEW. OLD commands target GPRs, new commands work only
with SPRs/CPRs. It's worth mentioning that at least legacy and v3 have different
encoding for OLD commands.

We will document only v3 encoding.

Generic encoding:
```c
struct cmdq_instruction {
	u16 arg_c:16; // low u16 of arg b
	u16 arg_b:16; // high u16 of arg b
	u16 arg_a:16; // low u16 of arg a
	u8 s_op:5;
	u8 arg_c_type:1;
	u8 arg_b_type:1;
	u8 arg_a_type:1; // start of arg a
	u8 op:8;
};
```

### OLD
Only GPRs are allowed. There's a linear mapping for R and P aliasing.
R0-R15 = 0-15. P0-P7 = 16-23

#### READ (0x01)
- opcode: `0x01`
- arg a: `addr_is_reg = a[23]`, `data_is_reg = a[22] (=1)`, `src = a[20:16]`, 
- arg b: `dst = LOW_U16(b)`

```c
u32 addr;

if (addr_is_reg)
  addr = GPR[src];
else
  arg = subsys2phys(src) + LOW_U16(a);

GPR[dst] = *addr;
```

Note: can use 64-bit reg for `GPR[src]`.

#### MOVE/MASK (0x02)
- opcode: `0x02`
- arg a: `s_op = a[23]`; `idx = a[20:16]`
- arg b: value

s_op: 0 = MASK, 1 = MOVE

If MASK => `R0 = ~value`, else
```c
u64 value_u48 = u48_from_u16(LOW_U16(a), HIGH_U16(b), LOW_U16(c));
GPR[idx] = value_u48;
```

Note: in the MOVE mode low u16 bits of the R1 are clobbered. It can be used for
bulk LOAD operation, otherwise R1 should be reserved or used as scratch
register.

UPD: I don't know if It's R1 itself or other reg grouped by P alias. This is
giving me headache. But you know what? NEVER USE R1 OR GET READY FOR HOURS OF
DEBUGGING.

#### WRITE (0x04)
- arg a: `addr_is_reg = a[23]`, `value_is_reg = a[22]`, `subsys = a[20:16]`,
`offset = LOW_U16(a)`, `mask_en = a[0]`
- arg b: `value = b` (full 32-bit imm) or `value_reg = b` (GPR id) selected by `value_is_reg`

```c
u32 addr;

if (addr_is_reg)
  addr = GPR[subsys];
else
  addr = subsys2phys(subsys) + (offset & ~1); // remove mask_en

u32 data = value_is_reg ? GPR[value_reg] : value;

mask_real = mask_en ? load_mask() : 0;
*addr = (*addr & mask_real) | (data & ~mask_real);
```

Note: `load_mask()` comes from the MASK instruction. Supports only u16.

#### POLL (0x08)
- arg a: `addr_type = a[23]`, `value_is_reg = a[22]`, `subsys = a[20:16]`, `offset = LOW_U16(a)`, `mask_en = a[0]`
- arg b: `expected = b` (full 32-bit imm) or `expected_reg = b` (GPR id) selected by `value_is_reg`

```c
u32 poll_addr;

if (addr_type)
  poll_addr = GPR[subsys]; // s_op doubles as GPR id here
else
  poll_addr = subsys2phys(subsys) + (offset & ~1); // remove mask_en

u32 expected = value_is_reg ? GPR[expected_reg] : expected;

mask_real = mask_en ? load_mask() : 0;
while ((*poll_addr ^ expected) & ~mask_real) {}
```

Note: `load_mask()` comes from the MASK instruction.

#### JUMP (0x10)
- opcode: `0x10`
- arg a: `is_abs = a[0]`, `by_reg = a[22]`
- arg b: `target = b`

```c
if (by_reg)
  unknown_state();
else if (is_abs)
  PC = target << shift;
else
  PC = PC + (s32)target;
```

Note: do NOT emit by_reg = 1. It doesn't work reliably. Instead use JUMP_C with
always true condition.

Note: shift = 0 on v2/v3 platform data, 3 on v4. JUMP +8 is NOP.

#### WFE/SYNC (0x20)
- opcode: `0x20`
- arg a: `event = a[9:0]`
- arg b: `upd = b[31]`, `upd_val = b[27:16]`, `wait = b[15]`, `wait_val = b[11:0]`

```c
if (wait && *event != wait_val) stall();
if (upd) *event = upd_val;  // 0 = clear, nonzero = set
```

`0x80008001` wait+clear, `0x80000000` clear, `0x80010000` set,
`0x00008001` wait-no-clear in `cmdq_core_interpret_instruction`.

#### EOC/MARKER (0x40)
- opcode: `0x40`
- arg a: `no_inc = a[16]`, `no_suspend = a[21]`
- arg b: `irq = b[0]`, `prefetch_en = b[16]`, `prefetch_en2 = b[17]`, `prefetch_mark = b[20]`

irq: 0 = MARKER, 1 = EOC

prefetch: enable prefetch engine. Size must be less than 96 on mt6765 to avoid
deadlock. Prefetch regions can be chained.

no_suspend: forbid GCE thread suspend by the AP.


```c
// MARKER en: (a, b) = (0x210000, 0x130000) -> no_inc=1, no_suspend=1,
//                     prefetch_mark=1, prefetch_en2=1, prefetch_en=1, irq=0.
// MARKER dis:(a, b) = (0x010000, 0x100000) -> no_inc=1, prefetch_mark=1.
if (irq)
  halt_and_raise_IRQ();
else
  EXEC_COUNTER += no_inc ? 0 : 1;

prefetch_apply();
```

### NEW
It uses unified register space for SPR, GPR and CPR access. Refer to the
register section for details.

#### READ_S (0x80)
Supports only SPR for address high bits.

- arg a: `dst_is_reg = a[23] (=1)`, `addr_high = a[20:16]`, `dst = LOW_U16(a)`
- arg b: `addr_low = HIGH_U16(b)`, `indirect = HIGH_U16(b)[1]` (word bit 17)

```c
u32 high;

if (indirect)
  high = SPR[addr_high];
else
  high = subsys2phys(addr_high);

u32 low  = addr_low & ~0x3u; // align to 32-bit boundary (clears flag bits)
u32 addr = (high << 16) | low;
SPR_GPR_CPR[dst] = *addr;
```

Note: must call LOGIC with ASSIGN before this instruction.

#### WRITE_S (0x90)
- arg a: `src_is_reg = a[22]`, `addr_high = a[20:16]`, `addr_low = LOW_U16(a)`
- arg b: `src = HIGH_U16(b)`, `src_lo = LOW_U16(b)`, `indirect = LOW_U16(a)[1]` (word bit 33)

```c
u32 high;
if (indirect)
  high = SPR[addr_high];
else
  high = subsys2phys(addr_high);

u32 low  = addr_low & ~0x3u; // align to 32-bit boundary (clears flag bits)
u32 addr = (high << 16) | low;
u32 data = src_is_reg ? SPR_GPR_CPR[src] : ((u32)src << 16) | src_lo;
*addr = data;
```

Note: must call LOGIC with ASSIGN before this instruction.

#### WRITE_S_W_MASK (0x91)
Same encoding as WRITE_S. Must invoke MASK before this instruction.

```c
u32 addr = /* same decode as WRITE_S */;
u32 data = /* same decode as WRITE_S */;
u32 mask = load_mask();
*addr = (*addr & mask) | (data & ~mask);
```

Note: `load_mask()` comes from the MASK instruction.

#### LOGIC (0xA0)
- arg a: `dst_is_reg = a[23] (=1)`, `left_is_reg = a[22]`, `right_is_reg = a[21]`,
`op = a[20:16]`, `dst = LOW_U16(a)`
- arg b: `left = HIGH_U16(b)`, `right = LOW_U16(b)`

| s_op | operation                                                                      |
|:----:|--------------------------------------------------------------------------------|
| 0    | `=` (assign imm32). Ignores *_is_reg flags, right and left are always imm16    |
| 1    | `+`                                                                            |
| 2    | `-`                                                                            |
| 3    | `*`                                                                            |
| 8    | `^`                                                                            |
| 9    | `~` (uses `left` only)                                                         |
| 10   | `|`                                                                            |
| 11   | `&`                                                                            |
| 12   | `<<`                                                                           |
| 13   | `>>` (logical, no sign extension)                                              |

```c
u32 b = left_is_reg && op != 0 ? SPR_GPR_CPR[left] : (u32)left;
u32 c = right_is_reg && op != 0 ? SPR_GPR_CPR[right] : (u32)right;
switch (op) {
case 0:
  SPR_GPR_CPR[dst] = (b << 16) | c;
  break;
case 9:
  SPR_GPR_CPR[dst] = ~b;
  break;
default:
  SPR_GPR_CPR[dst] = b <op> c;
  break;
}
```

#### Absolute JUMP_C (0xB0)/relative JUMP_C (0xB1)
Their encoding is the same except PC computation.

- arg a: `target_is_reg = a[23]`, `left_is_reg = a[22]`, `right_is_reg = a[21]`,
  `cond = a[20:16]`, `target_imm = LOW_U16(a)`, `target_reg = LOW_U16(a)`
- arg b: `left = HIGH_U16(b)`, `right = LOW_U16(b)`

| cond | operation |
| :--: | :-------: |
| 0    | ==        |
| 1    | !=        |
| 2    | >=        |
| 3    | <=        |
| 4    | >         |
| 5    | <         |

(all are u32)

```c
l = left_is_reg ? SPR_GPR_CPR[left] : left
r = right_is_reg ? SPR_GPR_CPR[right] : right;
if (l cond r) {
  if (target_is_reg)
    PC = SPR_GPR_CPR[target] << shift; 
  else
    PC = PC + (s32)(target_imm << shift);
}
```

Note: backward jump MUST be unconditional. For this use OLD JUMP instruction.

Note: shift = 0 on v2/v3 platform data, 3 on v4.
