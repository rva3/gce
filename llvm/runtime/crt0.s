// CPU loads the raw image at the exact address selected at static link time.
.section .text.start,"ax",@progbits
.p2align 3
.globl _start
.type _start,@function
_start:
jump #__gce_startup
.size _start, .-_start

// The linker places the indirect relay between entry and startup, so even
// main's return to __gce_park is forward from the relay.
.section .text.startup,"ax",@progbits
.p2align 3
.type __gce_startup,@function
__gce_startup:
ldi r14, __gce_stack_top
ldi r2, __gce_bss_start
ldi r3, __gce_bss_end
ldi r4, #0
.Lbss:
jeq .Lmain, r2, r3
write [r2], r4
add r2, r2, #4
jump .Lbss
.Lmain:
ldi r15, __gce_park
jump #main
.globl __gce_park
__gce_park:
jump +0
.size __gce_startup, .-__gce_startup

.section .text.__gce_trap,"ax",@progbits
.p2align 3
.globl __gce_trap
.type __gce_trap,@function
__gce_trap:
jump +0
.size __gce_trap, .-__gce_trap
