// A low-address relay makes every register-target JUMP_C strictly forward.
// Input R13 = physical target; clobbers only reserved SPR0/SPR1.
// Link this section before all callable code and return continuations.
.section .text.gce.indirect,"ax",@progbits
.p2align 3
.globl __gce_indirect
.hidden __gce_indirect
.type __gce_indirect,@function
__gce_indirect:
and spr1, r13, #7
jeq +16, spr1, #0
jump .Lbad_target
ldi spr0, .Ldispatch
jgt +16, r13, spr0
jump .Lbad_target
.Ldispatch:
jeq r13, #0, #0
.Lbad_target:
jump +0
.size __gce_indirect, .-__gce_indirect
