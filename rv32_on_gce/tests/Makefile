CLANG    := clang
CFLAGS   := --target=riscv32 -march=rv32i -mabi=ilp32 -nostdlib -ffreestanding \
            -fuse-ld=lld -Wl,-e,_start
SIM_BASE := 0x40000000
HW_BASE  := 0xa8000000

SRCS := hello.c hello2.c calls.c fib.c neg.c ops.c trap.c m.c amo.c csr.c clint.c umode.c misalign.c
SIM_ELFS := $(SRCS:%.c=build-sim/%.elf)
HW_ELFS  := $(SRCS:%.c=build-hw/%.elf)

MARCH_m.c := rv32im
MARCH_amo.c := rv32ia

# DO NOT USE make sim ! ! ! 

.PHONY: sim hw clean
sim: $(SIM_ELFS)
hw: $(HW_ELFS)

build-sim/%.elf: %.c
	mkdir -p build-sim
	$(CLANG) $(CFLAGS) -march=$(if $(MARCH_$<),$(MARCH_$<),rv32i) $(if $(filter ops.c,$<),-O1,-O2) $< -o $@ -Wl,-Ttext=$(SIM_BASE)

build-hw/%.elf: %.c
	mkdir -p build-hw
	$(CLANG) $(CFLAGS) -march=$(if $(MARCH_$<),$(MARCH_$<),rv32i) $(if $(filter ops.c,$<),-O1,-O2) $< -o $@ -Wl,-Ttext=$(HW_BASE)

clean:
	rm -rf build-sim build-hw
