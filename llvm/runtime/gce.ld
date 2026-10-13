/* Static ABI v1. Override addresses with --defsym before -T. */
OUTPUT_FORMAT(elf32-gce)
OUTPUT_ARCH(gce)
ENTRY(_start)
GCE_BASE = DEFINED(GCE_BASE) ? GCE_BASE : 0xf7600000;
GCE_CAPACITY = DEFINED(GCE_CAPACITY) ? GCE_CAPACITY : 0x01000000;
GCE_STACK_SIZE = DEFINED(GCE_STACK_SIZE) ? GCE_STACK_SIZE : 0x10000;
PHDRS { image PT_LOAD FLAGS(7); }
SECTIONS {
  . = GCE_BASE;
  .text : ALIGN(8) {
    KEEP(*(.text.start))
    KEEP(*(.text.gce.indirect))
    __gce_code_start = .;
    *(.text .text.*)
  } :image
  .rodata : ALIGN(8) { *(.rodata .rodata.*) } :image
  .data : ALIGN(8) { *(.data .data.*) } :image
  .image_end : { BYTE(0); . = ALIGN(8); __gce_image_end = .; } :image
  .bss (NOLOAD) : ALIGN(8) {
    __gce_bss_start = .; *(.bss .bss.*) *(COMMON)
    . = ALIGN(8); __gce_bss_end = .;
  } :image
  __gce_stack_bottom = GCE_BASE + GCE_CAPACITY - GCE_STACK_SIZE;
  .stack __gce_stack_bottom (NOLOAD) : {
    . += GCE_STACK_SIZE; __gce_stack_top = .;
  } :image
  /DISCARD/ : { *(.comment) *(.note.GNU-stack) *(.eh_frame .eh_frame.*) }
}
ASSERT((GCE_BASE & 7) == 0, "GCE_BASE must be 8-byte aligned")
ASSERT((GCE_CAPACITY & 7) == 0 && GCE_CAPACITY > 0, "invalid GCE_CAPACITY")
ASSERT((GCE_STACK_SIZE & 7) == 0 && GCE_STACK_SIZE > 0 && GCE_STACK_SIZE < GCE_CAPACITY, "invalid GCE_STACK_SIZE")
ASSERT(GCE_BASE + GCE_CAPACITY <= 0xffffffff, "DMA buffer/stack exceeds 32-bit address space")
ASSERT(__gce_bss_end <= __gce_stack_bottom, "GCE code/data/BSS overlaps reserved stack")
ASSERT(_start == GCE_BASE, "entry must be at DMA buffer offset zero")
ASSERT(__gce_indirect == GCE_BASE + 8, "indirect relay must immediately follow the entry jump")
ASSERT(__gce_code_start == GCE_BASE + 72, "unexpected indirect relay size or layout")
