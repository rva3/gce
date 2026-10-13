fetch:
add r0, cpr1t11, #0
add r0, r0, #1
and r0, r0, #0x7ff
add cpr1t11, r0, #0
jeq tmr_check, r0, #0
read r14, [r15]
and r13, r14, #0x7f
jeq op_lui, r13, #0x37
jeq op_auipc, r13, #0x17
jeq op_jal, r13, #0x6f
jeq op_jalr, r13, #0x67
jeq op_branch, r13, #0x63
jeq op_load, r13, #0x03
jeq op_store, r13, #0x23
jeq op_imm, r13, #0x13
jeq op_op, r13, #0x33
jeq op_fence, r13, #0x0f
jeq op_system, r13, #0x73
jeq op_amo, r13, #0x2f
jump trap_udf

op_lui:
lsr r12, r14, #7
and r12, r12, #0x1f
ldi s0, #0xfffff000
and r5, r14, s0
ldi s3, ret_lui
ldi s0, tbl_rd
lsl s1, r12, #4
add s2, s0, s1
jeq s2, r0, r0

ret_lui:
add r15, r15, #4
jump fetch

op_auipc:
lsr r12, r14, #7
and r12, r12, #0x1f
ldi s0, #0xfffff000
and r5, r14, s0
add r5, r5, r15
ldi s3, ret_auipc
ldi s0, tbl_rd
lsl s1, r12, #4
add s2, s0, s1
jeq s2, r0, r0

ret_auipc:
add r15, r15, #4
jump fetch
op_jal:
lsr r12, r14, #7
and r12, r12, #0x1f
lsr r0, r14, #31
lsl r0, r0, #20
lsr r5, r14, #21
and r5, r5, #0x3ff
lsl r5, r5, #1
lsr r2, r14, #20
and r2, r2, #1
lsl r2, r2, #11
lsr r3, r14, #12
and r3, r3, #0xff
lsl r3, r3, #12
orr r4, r0, r5
orr r4, r4, r2
orr r4, r4, r3
ldi s0, #0x100000
jge jal_neg, r4, s0
jeq jal_imm_done, r0, r0

jal_neg:
ldi s0, #0xffe00000
orr r4, r4, s0

jal_imm_done:
add r5, r15, #4
ldi s3, ret_jal
ldi s0, tbl_rd
lsl s1, r12, #4
add s2, s0, s1
jeq s2, r0, r0

ret_jal:
add r15, r15, r4
jump fetch
op_jalr:
lsr r12, r14, #7
and r12, r12, #0x1f
lsr r11, r14, #15
and r11, r11, #0x1f
lsr r4, r14, #20
jge jalr_neg, r4, #0x800
jeq jalr_imm_done, r0, r0

jalr_neg:
ldi s0, #0xfffff000
orr r4, r4, s0

jalr_imm_done:
ldi s3, ret_jalr_rs1
ldi s0, tbl_rs1
lsl s1, r11, #4
add s2, s0, s1
jeq s2, r0, r0

ret_jalr_rs1:
add r3, r7, r4
ldi s0, #0xfffffffe
and r3, r3, s0
add r5, r15, #4
ldi s3, ret_jalr
ldi s0, tbl_rd
lsl s1, r12, #4
add s2, s0, s1
jeq s2, r0, r0

ret_jalr:
add r15, r3, #0
jump fetch
op_branch:
lsr r11, r14, #15
and r11, r11, #0x1f
lsr r10, r14, #20
and r10, r10, #0x1f
lsr r9, r14, #12
and r9, r9, #0x7
lsr r0, r14, #31
lsl r0, r0, #12
lsr r5, r14, #25
and r5, r5, #0x3f
lsl r5, r5, #5
lsr r2, r14, #8
and r2, r2, #0xf
lsl r2, r2, #1
lsr r3, r14, #7
and r3, r3, #1
lsl r3, r3, #11
orr r4, r0, r5
orr r4, r4, r2
orr r4, r4, r3
jge br_neg, r4, #0x1000
jeq br_imm_done, r0, r0

br_neg:
ldi s0, #0xffffe000
orr r4, r4, s0

br_imm_done:
ldi s3, ret_br_rs1
ldi s0, tbl_rs1
lsl s1, r11, #4
add s2, s0, s1
jeq s2, r0, r0

ret_br_rs1:
ldi s3, ret_br_rs2
ldi s0, tbl_rs2
lsl s1, r10, #4
add s2, s0, s1
jeq s2, r0, r0

ret_br_rs2:
jeq br_beq, r9, #0x0
jeq br_bne, r9, #0x1
jeq br_blt, r9, #0x4
jeq br_bge, r9, #0x5
jeq br_bltu, r9, #0x6
jeq br_bgeu, r9, #0x7
jump trap_udf

br_beq:
jeq br_taken, r7, r6
jeq br_notaken, r0, r0

br_bne:
jne br_taken, r7, r6
jeq br_notaken, r0, r0

br_bltu:
jlt br_taken, r7, r6
jeq br_notaken, r0, r0

br_bgeu:
jge br_taken, r7, r6
jeq br_notaken, r0, r0

br_blt:
ldi s0, #0x80000000
xor r0, r7, s0
xor r2, r6, s0
jlt br_taken, r0, r2
jeq br_notaken, r0, r0

br_bge:
ldi s0, #0x80000000
xor r0, r7, s0
xor r2, r6, s0
jge br_taken, r0, r2
jeq br_notaken, r0, r0

br_taken:
add r15, r15, r4
jump fetch

br_notaken:
add r15, r15, #4
jump fetch
op_load:
lsr r12, r14, #7
and r12, r12, #0x1f
lsr r11, r14, #15
and r11, r11, #0x1f
lsr r9, r14, #12
and r9, r9, #0x7
lsr r4, r14, #20
jge load_neg, r4, #0x800
jeq load_imm_done, r0, r0

load_neg:
ldi s0, #0xfffff000
orr r4, r4, s0

load_imm_done:
ldi s3, ret_load_rs1
ldi s0, tbl_rs1
lsl s1, r11, #4
add s2, s0, s1
jeq s2, r0, r0

ret_load_rs1:
add r3, r7, r4
lsr r4, r3, #16
jeq clint_load, r4, #0x200
jeq load_lb, r9, #0x0
jeq load_lh, r9, #0x1
jeq load_lw, r9, #0x2
jeq load_lbu, r9, #0x4
jeq load_lhu, r9, #0x5
jump trap_udf

load_lw:
and r0, r3, #3
jne load_lw_mis, r0, #0
read r5, [r3]
jeq load_wb, r0, r0
load_lw_mis:
ldi r5, #4
jump trap_enter_r3

load_lbu:
and r0, r3, #3
lsr r3, r3, #2
lsl r3, r3, #2
read r2, [r3]
lsl r0, r0, #3
lsr r5, r2, r0
and r5, r5, #0xff
jeq load_wb, r0, r0

load_lb:
and r0, r3, #3
lsr r3, r3, #2
lsl r3, r3, #2
read r2, [r3]
lsl r0, r0, #3
lsr r5, r2, r0
and r5, r5, #0xff
and r0, r5, #0x80
jeq load_wb, r0, #0
ldi s0, #0xffffff00
orr r5, r5, s0
jeq load_wb, r0, r0

load_lhu:
and r0, r3, #1
jne load_lhu_mis, r0, #0
and r0, r3, #2
lsr r3, r3, #2
lsl r3, r3, #2
read r2, [r3]
lsl r0, r0, #3
lsr r5, r2, r0
and r5, r5, #0xffff
jeq load_wb, r0, r0
load_lhu_mis:
ldi r5, #4
jump trap_enter_r3

load_lh:
and r0, r3, #1
jne load_lh_mis, r0, #0
and r0, r3, #2
lsr r3, r3, #2
lsl r3, r3, #2
read r2, [r3]
lsl r0, r0, #3
lsr r5, r2, r0
and r5, r5, #0xffff
and r0, r5, #0x8000
jeq load_wb, r0, #0
ldi s0, #0xffff0000
orr r5, r5, s0
jeq load_wb, r0, r0
load_lh_mis:
ldi r5, #4
jump trap_enter_r3

load_wb:
ldi s3, ret_load
ldi s0, tbl_rd
lsl s1, r12, #4
add s2, s0, s1
jeq s2, r0, r0

ret_load:
add r15, r15, #4
jump fetch
op_store:
lsr r11, r14, #15
and r11, r11, #0x1f
lsr r10, r14, #20
and r10, r10, #0x1f
lsr r9, r14, #12
and r9, r9, #0x7
lsr r0, r14, #25
lsl r0, r0, #5
lsr r5, r14, #7
and r5, r5, #0x1f
orr r4, r0, r5
jge store_neg, r4, #0x800
jeq store_imm_done, r0, r0

store_neg:
ldi s0, #0xfffff000
orr r4, r4, s0

store_imm_done:
ldi s3, ret_store_rs1
ldi s0, tbl_rs1
lsl s1, r11, #4
add s2, s0, s1
jeq s2, r0, r0

ret_store_rs1:
ldi s3, ret_store_rs2
ldi s0, tbl_rs2
lsl s1, r10, #4
add s2, s0, s1
jeq s2, r0, r0

ret_store_rs2:
add r3, r7, r4
lsr r4, r3, #16
jeq clint_store, r4, #0x200
jeq store_sb, r9, #0x0
jeq store_sh, r9, #0x1
jeq store_sw, r9, #0x2
jump trap_udf

store_sw:
and r0, r3, #3
jne store_sw_mis, r0, #0
write [r3], r6
jeq store_done, r0, r0
store_sw_mis:
ldi r5, #6
jump trap_enter_r3

store_sb:
and r0, r3, #3
lsr r3, r3, #2
lsl r3, r3, #2
read r2, [r3]
lsl r0, r0, #3
ldi s0, #0xff
lsl s0, s0, r0
not s0, s0
and r2, r2, s0
and s0, r6, #0xff
lsl s0, s0, r0
orr r2, r2, s0
write [r3], r2
jeq store_done, r0, r0

store_sh:
and r0, r3, #1
jne store_sh_mis, r0, #0
and r0, r3, #2
lsr r3, r3, #2
lsl r3, r3, #2
read r2, [r3]
lsl r0, r0, #3
ldi s0, #0xffff
lsl s0, s0, r0
not s0, s0
and r2, r2, s0
and s0, r6, #0xffff
lsl s0, s0, r0
orr r2, r2, s0
write [r3], r2
jeq store_done, r0, r0
store_sh_mis:
ldi r5, #6
jump trap_enter_r3

store_done:
add r15, r15, #4
jump fetch

clint_load:
jne trap_udf, r9, #0x2
and r0, r3, #0xffff
jeq clint_mtime, r0, #0xbff8
jeq clint_mtimeh, r0, #0xbffc
jeq clint_cmp, r0, #0x4000
jeq clint_cmph, r0, #0x4004
jump trap_udf
clint_mtime:
ldi r2, #0x10008048
read r5, [r2]
jump load_wb
clint_mtimeh:
ldi r5, #0
jump load_wb
clint_cmp:
add r5, cpr0t11, #0
jump load_wb
clint_cmph:
ldi r5, #0
jump load_wb
clint_store:
jne trap_udf, r9, #0x2
and r0, r3, #0xffff
jeq clnt_cmpw, r0, #0x4000
jeq clnt_cmphw, r0, #0x4004
jump trap_udf
clnt_cmpw:
add cpr0t11, r6, #0
add r0, cpr0t10, #0
ldi s0, #0xffffff7f
and r0, r0, s0
add cpr0t10, r0, #0
jump store_done
clnt_cmphw:
jump store_done
op_imm:
lsr r12, r14, #7
and r12, r12, #0x1f
lsr r11, r14, #15
and r11, r11, #0x1f
lsr r9, r14, #12
and r9, r9, #0x7
lsr r10, r14, #20
and r10, r10, #0x1f
ldi s3, ret_imm_rs1
ldi s0, tbl_rs1
lsl s1, r11, #4
add s2, s0, s1
jeq s2, r0, r0

ret_imm_rs1:
jeq imm_addi, r9, #0x0
jeq imm_slti, r9, #0x2
jeq imm_sltiu, r9, #0x3
jeq imm_xori, r9, #0x4
jeq imm_ori, r9, #0x6
jeq imm_andi, r9, #0x7
jeq imm_slli, r9, #0x1
jeq imm_srli_srai, r9, #0x5
jump trap_udf

imm_addi:
lsr r4, r14, #20
jge imm_addi_neg, r4, #0x800
jeq imm_addi_done, r0, r0

imm_addi_neg:
ldi s0, #0xfffff000
orr r4, r4, s0

imm_addi_done:
add r5, r7, r4
jeq imm_wb, r0, r0

imm_slti:
lsr r4, r14, #20
jge imm_slti_neg, r4, #0x800
jeq imm_slti_done, r0, r0

imm_slti_neg:
ldi s0, #0xfffff000
orr r4, r4, s0

imm_slti_done:
ldi s0, #0x80000000
xor r0, r7, s0
xor r2, r4, s0
jlt imm_one, r0, r2
ldi r5, #0
jeq imm_wb, r0, r0

imm_sltiu:
lsr r4, r14, #20
jge imm_sltiu_neg, r4, #0x800
jeq imm_sltiu_done, r0, r0

imm_sltiu_neg:
ldi s0, #0xfffff000
orr r4, r4, s0

imm_sltiu_done:
jlt imm_one, r7, r4
ldi r5, #0
jeq imm_wb, r0, r0

imm_xori:
lsr r4, r14, #20
jge imm_xori_neg, r4, #0x800
jeq imm_xori_done, r0, r0

imm_xori_neg:
ldi s0, #0xfffff000
orr r4, r4, s0

imm_xori_done:
xor r5, r7, r4
jeq imm_wb, r0, r0

imm_ori:
lsr r4, r14, #20
jge imm_ori_neg, r4, #0x800
jeq imm_ori_done, r0, r0

imm_ori_neg:
ldi s0, #0xfffff000
orr r4, r4, s0

imm_ori_done:
orr r5, r7, r4
jeq imm_wb, r0, r0

imm_andi:
lsr r4, r14, #20
jge imm_andi_neg, r4, #0x800
jeq imm_andi_done, r0, r0

imm_andi_neg:
ldi s0, #0xfffff000
orr r4, r4, s0

imm_andi_done:
and r5, r7, r4
jeq imm_wb, r0, r0

imm_slli:
lsl r5, r7, r10
jeq imm_wb, r0, r0

imm_srli_srai:
lsr r0, r14, #30
and r0, r0, #1
jeq imm_srai, r0, #1
lsr r5, r7, r10
jeq imm_wb, r0, r0

imm_srai:
lsr r5, r7, r10
ldi s0, #0x80000000
jge imm_srai_neg, r7, s0
jeq imm_wb, r0, r0

imm_srai_neg:
ldi r2, #32
sub r3, r2, r10
ldi r0, #0xffffffff
lsl r0, r0, r3
orr r5, r5, r0
jeq imm_wb, r0, r0

imm_one:
ldi r5, #1

imm_wb:
ldi s3, ret_imm
ldi s0, tbl_rd
lsl s1, r12, #4
add s2, s0, s1
jeq s2, r0, r0

ret_imm:
add r15, r15, #4
jump fetch
op_op:
lsr r12, r14, #7
and r12, r12, #0x1f
lsr r11, r14, #15
and r11, r11, #0x1f
lsr r10, r14, #20
and r10, r10, #0x1f
lsr r9, r14, #12
and r9, r9, #0x7
lsr r8, r14, #25
and r8, r8, #0x7f
jeq op_base, r8, #0
jeq op_subsra, r8, #0x20
jeq op_m, r8, #0x1
jump trap_udf

op_subsra:
jeq op_sub, r9, #0x0
jeq op_sra, r9, #0x5
jump trap_udf

op_m:
ldi s3, ret_m_rs1
ldi s0, tbl_rs1
lsl s1, r11, #4
add s2, s0, s1
jeq s2, r0, r0
ret_m_rs1:
ldi s3, ret_m_rs2
ldi s0, tbl_rs2
lsl s1, r10, #4
add s2, s0, s1
jeq s2, r0, r0
ret_m_rs2:
jeq op_mul, r9, #0x0
jeq op_mulh, r9, #0x1
jeq op_mulhsu, r9, #0x2
jeq op_mulhu, r9, #0x3
jeq op_div, r9, #0x4
jeq op_divu, r9, #0x5
jeq op_rem, r9, #0x6
jeq op_remu, r9, #0x7
jump trap_udf

op_mul:
mul r5, r7, r6
jeq op_wb, r0, r0

op_mulhu_core:
and r0, r7, #0xffff
lsr r2, r7, #16
and r3, r6, #0xffff
lsr r4, r6, #16
mul r6, r0, r3
mul r7, r0, r4
mul r0, r2, r3
mul r2, r2, r4
lsr r6, r6, #16
and r3, r7, #0xffff
add r6, r6, r3
and r3, r0, #0xffff
lsr r4, r0, #16
add r6, r6, r3
lsr r3, r6, #16
lsr r6, r7, #16
add r5, r2, r3
add r5, r5, r6
add r5, r5, r4
jeq s3, r0, r0

op_mulhu:
ldi s3, ret_mulhu
jump op_mulhu_core
ret_mulhu:
jeq op_wb, r0, r0

op_mulh:
ldi r9, #0x0
mul r11, r7, r6
ldi r4, #0x80000000
jge mh_a_neg, r7, r4
jeq mh_a_pos, r0, r0
mh_a_neg:
ldi r0, #0x0
sub r7, r0, r7
ldi r9, #0x1
mh_a_pos:
jge mh_b_neg, r6, r4
jeq mh_b_pos, r0, r0
mh_b_neg:
ldi r0, #0x0
sub r6, r0, r6
xor r9, r9, #0x1
mh_b_pos:
ldi s3, ret_mulh
jump op_mulhu_core
ret_mulh:
jeq op_wb, r9, #0
ldi r0, #0x0
sub r5, r0, r5
jeq op_wb, r11, #0
ldi r0, #0x1
sub r5, r5, r0
jeq op_wb, r0, r0

op_mulhsu:
ldi r9, #0x0
mul r11, r7, r6
ldi r4, #0x80000000
jge mhs_neg, r7, r4
jeq mhs_pos, r0, r0
mhs_neg:
ldi r0, #0x0
sub r7, r0, r7
ldi r9, #0x1
mhs_pos:
ldi s3, ret_mulhsu
jump op_mulhu_core
ret_mulhsu:
jeq op_wb, r9, #0
ldi r0, #0x0
sub r5, r0, r5
jeq op_wb, r11, #0
ldi r0, #0x1
sub r5, r5, r0
jeq op_wb, r0, r0

divcore:
ldi r3, #0x0
ldi r5, #0x0
ldi r2, #31
div_loop:
lsr r0, r7, r2
and r0, r0, #1
lsl r3, r3, #1
orr r3, r3, r0
jge div_sub, r3, r6
jeq div_next, r0, r0
div_sub:
sub r3, r3, r6
ldi r0, #0x1
lsl r0, r0, r2
orr r5, r5, r0
div_next:
jeq div_done, r2, #0
sub r2, r2, #1
jump div_loop
div_done:
jeq s3, r0, r0

op_divu:
jeq divu_zero, r6, #0
ldi s3, ret_divu
jump divcore
ret_divu:
jeq op_wb, r0, r0
divu_zero:
ldi r5, #0xffffffff
jeq op_wb, r0, r0

op_remu:
jeq remu_zero, r6, #0
ldi s3, ret_remu
jump divcore
ret_remu:
add r5, r3, #0
jeq op_wb, r0, r0
remu_zero:
add r5, r7, #0
jeq op_wb, r0, r0

op_div:
ldi r4, #0x80000000
jeq div_b0, r6, #0
ldi r0, #0xffffffff
jne div_general, r6, r0
jne div_general, r7, r4
add r5, r4, #0
jeq op_wb, r0, r0
div_b0:
ldi r5, #0xffffffff
jeq op_wb, r0, r0
div_general:
ldi r9, #0x0
jge div_a_neg, r7, r4
jeq div_a_pos, r0, r0
div_a_neg:
ldi r0, #0x0
sub r7, r0, r7
ldi r9, #0x1
div_a_pos:
jge div_b_neg, r6, r4
jeq div_b_pos, r0, r0
div_b_neg:
ldi r0, #0x0
sub r6, r0, r6
xor r9, r9, #0x1
div_b_pos:
ldi s3, ret_div
jump divcore
ret_div:
jeq op_wb, r9, #0
ldi r0, #0x0
sub r5, r0, r5
jeq op_wb, r0, r0

op_rem:
ldi r4, #0x80000000
jeq rem_b0, r6, #0
ldi r0, #0xffffffff
jne rem_general, r6, r0
jne rem_general, r7, r4
ldi r5, #0x0
jeq op_wb, r0, r0
rem_b0:
add r5, r7, #0
jeq op_wb, r0, r0
rem_general:
ldi r9, #0x0
jge rem_a_neg, r7, r4
jeq rem_a_pos, r0, r0
rem_a_neg:
ldi r0, #0x0
sub r7, r0, r7
ldi r9, #0x1
rem_a_pos:
jge rem_b_neg, r6, r4
jeq rem_b_pos, r0, r0
rem_b_neg:
ldi r0, #0x0
sub r6, r0, r6
rem_b_pos:
ldi s3, ret_rem
jump divcore
ret_rem:
add r5, r3, #0
jeq op_wb, r9, #0
ldi r0, #0x0
sub r5, r0, r5
jeq op_wb, r0, r0

op_base:
ldi s3, ret_op_rs1
ldi s0, tbl_rs1
lsl s1, r11, #4
add s2, s0, s1
jeq s2, r0, r0

ret_op_rs1:
ldi s3, ret_op_rs2
ldi s0, tbl_rs2
lsl s1, r10, #4
add s2, s0, s1
jeq s2, r0, r0

ret_op_rs2:
jeq op_add, r9, #0x0
jeq op_sll, r9, #0x1
jeq op_slt, r9, #0x2
jeq op_sltu, r9, #0x3
jeq op_xor, r9, #0x4
jeq op_srl, r9, #0x5
jeq op_or, r9, #0x6
jeq op_and, r9, #0x7
jump trap_udf

op_add:
add r5, r7, r6
jeq op_wb, r0, r0

op_sub:
ldi s3, ret_sub_rs1
ldi s0, tbl_rs1
lsl s1, r11, #4
add s2, s0, s1
jeq s2, r0, r0

ret_sub_rs1:
ldi s3, ret_sub_rs2
ldi s0, tbl_rs2
lsl s1, r10, #4
add s2, s0, s1
jeq s2, r0, r0

ret_sub_rs2:
sub r5, r7, r6
jeq op_wb, r0, r0

op_sll:
and r0, r6, #0x1f
lsl r5, r7, r0
jeq op_wb, r0, r0

op_slt:
ldi s0, #0x80000000
xor r0, r7, s0
xor r2, r6, s0
jlt op_one, r0, r2
ldi r5, #0
jeq op_wb, r0, r0

op_sltu:
jlt op_one, r7, r6
ldi r5, #0
jeq op_wb, r0, r0

op_xor:
xor r5, r7, r6
jeq op_wb, r0, r0

op_srl:
and r0, r6, #0x1f
lsr r5, r7, r0
jeq op_wb, r0, r0

op_sra:
ldi s3, ret_sra_rs1
ldi s0, tbl_rs1
lsl s1, r11, #4
add s2, s0, s1
jeq s2, r0, r0

ret_sra_rs1:
ldi s3, ret_sra_rs2
ldi s0, tbl_rs2
lsl s1, r10, #4
add s2, s0, s1
jeq s2, r0, r0

ret_sra_rs2:
and r0, r6, #0x1f
lsr r5, r7, r0
ldi s0, #0x80000000
jge op_sra_neg, r7, s0
jeq op_wb, r0, r0

op_sra_neg:
ldi r2, #32
sub r3, r2, r0
ldi r0, #0xffffffff
lsl r0, r0, r3
orr r5, r5, r0
jeq op_wb, r0, r0

op_or:
orr r5, r7, r6
jeq op_wb, r0, r0

op_and:
and r5, r7, r6
jeq op_wb, r0, r0

op_one:
ldi r5, #1

op_wb:
ldi s3, ret_op
ldi s0, tbl_rd
lsl s1, r12, #4
add s2, s0, s1
jeq s2, r0, r0

ret_op:
add r15, r15, #4
jump fetch

op_amo:
lsr r12, r14, #7
and r12, r12, #0x1f
lsr r11, r14, #15
and r11, r11, #0x1f
lsr r10, r14, #20
and r10, r10, #0x1f
lsr r9, r14, #12
and r9, r9, #0x7
jeq amo_f3ok, r9, #0x2
jump trap_udf
amo_f3ok:
ldi s3, ret_amo_rs1
ldi s0, tbl_rs1
lsl s1, r11, #4
add s2, s0, s1
jeq s2, r0, r0
ret_amo_rs1:
and r0, r7, #3
jeq amo_aligned, r0, #0
jump trap_udf
amo_aligned:
ldi s3, ret_amo_rs2
ldi s0, tbl_rs2
lsl s1, r10, #4
add s2, s0, s1
jeq s2, r0, r0
ret_amo_rs2:
lsr r8, r14, #27
and r8, r8, #0x1f
jeq amo_add, r8, #0x0
jeq amo_swap, r8, #0x1
jeq amo_xor, r8, #0x4
jeq amo_and, r8, #0xc
jeq amo_or, r8, #0x8
jeq amo_min, r8, #0x10
jeq amo_max, r8, #0x14
jeq amo_minu, r8, #0x18
jeq amo_maxu, r8, #0x1c
jeq amo_lr, r8, #0x2
jeq amo_sc, r8, #0x3
jump trap_udf

amo_swap:
read r3, [r7]
add r5, r6, #0
write [r7], r5
add r5, r3, #0
jump op_wb

amo_add:
read r3, [r7]
add r5, r3, r6
write [r7], r5
add r5, r3, #0
jump op_wb

amo_xor:
read r3, [r7]
xor r5, r3, r6
write [r7], r5
add r5, r3, #0
jump op_wb

amo_and:
read r3, [r7]
and r5, r3, r6
write [r7], r5
add r5, r3, #0
jump op_wb

amo_or:
read r3, [r7]
orr r5, r3, r6
write [r7], r5
add r5, r3, #0
jump op_wb

amo_minu:
read r3, [r7]
jlt amo_minu_old, r3, r6
add r5, r6, #0
jeq amo_minu_done, r0, r0
amo_minu_old:
add r5, r3, #0
amo_minu_done:
write [r7], r5
add r5, r3, #0
jump op_wb

amo_maxu:
read r3, [r7]
jge amo_maxu_old, r3, r6
add r5, r6, #0
jeq amo_maxu_done, r0, r0
amo_maxu_old:
add r5, r3, #0
amo_maxu_done:
write [r7], r5
add r5, r3, #0
jump op_wb

amo_min:
read r3, [r7]
ldi s0, #0x80000000
xor r0, r3, s0
xor r2, r6, s0
jlt amo_min_old, r0, r2
add r5, r6, #0
jeq amo_min_done, r0, r0
amo_min_old:
add r5, r3, #0
amo_min_done:
write [r7], r5
add r5, r3, #0
jump op_wb

amo_max:
read r3, [r7]
ldi s0, #0x80000000
xor r0, r3, s0
xor r2, r6, s0
jge amo_max_old, r0, r2
add r5, r6, #0
jeq amo_max_done, r0, r0
amo_max_old:
add r5, r3, #0
amo_max_done:
write [r7], r5
add r5, r3, #0
jump op_wb

amo_lr:
jeq amo_lr_ok, r10, #0
jump trap_udf
amo_lr_ok:
read r3, [r7]
add cpr0t8, r7, #0
ldi r0, #0x1
add cpr1t8, r0, #0
add r5, r3, #0
jump op_wb

amo_sc:
add r0, cpr0t8, #0
jne amo_sc_fail, r0, r7
add r0, cpr1t8, #0
jeq amo_sc_fail, r0, #0
write [r7], r6
ldi r0, #0x0
add cpr1t8, r0, #0
ldi r5, #0x0
jump op_wb
amo_sc_fail:
ldi r0, #0x0
add cpr1t8, r0, #0
ldi r5, #0x1
jump op_wb

op_fence:
add r15, r15, #4
jump fetch

op_system:
lsr r12, r14, #7
and r12, r12, #0x1f
lsr r11, r14, #15
and r11, r11, #0x1f
lsr r10, r14, #20
lsr r9, r14, #12
and r9, r9, #0x7
jeq sys_priv, r9, #0x0
jeq sys_rw, r9, #0x1
jeq sys_rs, r9, #0x2
jeq sys_rc, r9, #0x3
jeq sys_rwi, r9, #0x5
jeq sys_rsi, r9, #0x6
jeq sys_rci, r9, #0x7
jump trap_udf

sys_priv:
jeq sys_mret, r10, #0x302
jeq sys_wfi, r10, #0x105
jeq sys_ebreak, r10, #0x1
jeq sys_ecall, r10, #0x0
lsr r0, r10, #5
jeq sys_sfence_ok, r0, #0x9
jump trap_udf

sys_ecall:
add r0, cpr1t10, #0
jeq sys_ecall_m, r0, #0
sys_ecall_u:
ldi r5, #8
jump trap_enter
sys_ecall_m:
add r7, cpr1t4, #0
jeq sbi_shutdown, r7, #0x8
jeq sbi_legtimer, r7, #0x0
ldi s0, #0x10
jeq sbi_base, r7, s0
ldi s0, #0x54494d45
jeq sbi_time, r7, s0
ldi r5, #11
jump trap_enter
sbi_shutdown:
jump +0
sbi_legtimer:
add r0, cpr0t4, #0
jne sbi_bad, r0, #0
jump sbi_dotimer
sbi_time:
add r0, cpr0t4, #0
jne sbi_bad, r0, #0
sbi_dotimer:
add r0, cpr2t2, #0
add cpr0t11, r0, #0
add r0, cpr0t10, #0
ldi s0, #0xffffff7f
and r0, r0, s0
add cpr0t10, r0, #0
ldi r0, #0
add cpr2t2, r0, #0
add cpr3t2, r0, #0
add r15, r15, #4
jump fetch
sbi_bad:
ldi r5, #11
jump trap_enter
sbi_base:
add r0, cpr0t4, #0
jeq sbi_b_spec, r0, #0
jeq sbi_b_implid, r0, #0x1
jeq sbi_b_implver, r0, #0x2
jeq sbi_b_probe, r0, #0x3
ldi r5, #11
jump trap_enter
sbi_b_spec:
ldi r0, #2
add cpr2t2, r0, #0
ldi r0, #0
add cpr3t2, r0, #0
add r15, r15, #4
jump fetch
sbi_b_implid:
ldi r0, #0
add cpr2t2, r0, #0
add cpr3t2, r0, #0
add r15, r15, #4
jump fetch
sbi_b_implver:
ldi r0, #0
add cpr2t2, r0, #0
add cpr3t2, r0, #0
add r15, r15, #4
jump fetch
sbi_b_probe:
add r0, cpr2t2, #0
ldi s0, #0x54494d45
jeq sbi_b_yes, r0, s0
ldi s0, #0x10
jeq sbi_b_yes, r0, s0
ldi r0, #0
add cpr2t2, r0, #0
add cpr3t2, r0, #0
add r15, r15, #4
jump fetch
sbi_b_yes:
ldi r0, #1
add cpr2t2, r0, #0
ldi r0, #0
add cpr3t2, r0, #0
add r15, r15, #4
jump fetch
sys_ebreak:
ldi r5, #3
jump trap_enter
sys_wfi:
add r15, r15, #4
jump fetch
sys_sfence_ok:
add r15, r15, #4
jump fetch
sys_mret:
add r15, cpr0t9, #0
add r0, cpr2t8, #0
and r2, r0, #0x80
lsr r2, r2, #4
ldi s0, #0xffffff77
and r0, r0, s0
orr r0, r0, r2
orr r0, r0, #0x80
add cpr2t8, r0, #0
jump fetch

trap_enter:
add cpr0t9, r15, #0
add cpr1t9, r5, #0
ldi r0, #0
add cpr2t9, r0, #0
add r3, r0, #0
jump trap_common
trap_enter_r3:
add cpr0t9, r15, #0
add cpr1t9, r5, #0
add cpr2t9, r3, #0
jump trap_common
trap_common:
add r0, cpr2t8, #0
and r2, r0, #0x8
lsl r2, r2, #4
ldi s0, #0xffffff77
and r0, r0, s0
orr r0, r0, r2
add r2, cpr1t10, #0
jeq mpp_m, r2, #0
ldi s0, #0xffffe7ff
and r0, r0, s0
jeq mpp_done, r0, r0
mpp_m:
orr r0, r0, #0x1800
mpp_done:
add cpr2t8, r0, #0
tw_mepc: add r2, cpr0t9, #0
jeq tw_mepc_ok, r2, r15
jump tw_mepc
tw_mepc_ok:
tw_mcause: add r2, cpr1t9, #0
jeq tw_mcause_ok, r2, r5
jump tw_mcause
tw_mcause_ok:
tw_mtval: add r2, cpr2t9, #0
jeq tw_mtval_ok, r2, r3
jump tw_mtval
tw_mtval_ok:
tw_mstatus: add r2, cpr2t8, #0
jeq tw_mstatus_ok, r2, r0
jump tw_mstatus
tw_mstatus_ok:
add r15, cpr3t8, #0
jump fetch

sys_rw:
ldi s3, ret_csr_rs1
ldi s0, tbl_rs1
lsl s1, r11, #4
add s2, s0, s1
jeq s2, r0, r0
ret_csr_rs1:
ldi r6, #1
jump sys_csr
sys_rs:
ldi s3, ret_csr_rs1b
ldi s0, tbl_rs1
lsl s1, r11, #4
add s2, s0, s1
jeq s2, r0, r0
ret_csr_rs1b:
ldi r6, #0
jeq sys_csr, r11, #0
ldi r6, #1
jump sys_csr
sys_rc:
ldi s3, ret_csr_rs1c
ldi s0, tbl_rs1
lsl s1, r11, #4
add s2, s0, s1
jeq s2, r0, r0
ret_csr_rs1c:
ldi r6, #0
jeq sys_csr, r11, #0
ldi r6, #1
jump sys_csr
sys_rwi:
add r7, r11, #0
ldi r6, #1
jump sys_csr
sys_rsi:
add r7, r11, #0
ldi r6, #0
jeq sys_csr, r11, #0
ldi r6, #1
jump sys_csr
sys_rci:
add r7, r11, #0
ldi r6, #0
jeq sys_csr, r11, #0
ldi r6, #1
jump sys_csr

sys_csr:
jeq csr_mstatus, r10, #0x300
jeq csr_misa, r10, #0x301
jeq csr_medeleg, r10, #0x302
jeq csr_mideleg, r10, #0x303
jeq csr_mie, r10, #0x304
jeq csr_mtvec, r10, #0x305
jeq csr_mscratch, r10, #0x340
jeq csr_mepc, r10, #0x341
jeq csr_mcause, r10, #0x342
jeq csr_mtval, r10, #0x343
jeq csr_mip, r10, #0x344
jeq csr_mcycle, r10, #0xb00
jeq csr_mcycleh, r10, #0xb80
jeq csr_minstret, r10, #0xb02
jeq csr_minstreth, r10, #0xb82
jeq csr_cycle, r10, #0xc00
jeq csr_time, r10, #0xc01
jeq csr_instret, r10, #0xc02
jeq csr_zero, r10, #0xc80
jeq csr_zero, r10, #0xc81
jeq csr_zero, r10, #0xc82
jeq csr_zero, r10, #0xf11
jeq csr_zero, r10, #0xf12
jeq csr_zero, r10, #0xf13
jeq csr_zero, r10, #0xf14
jeq csr_zero, r10, #0xf15
jeq csr_pmpcfg0, r10, #0x3a0
jeq csr_pmpcfg1, r10, #0x3a1
jeq csr_pmpaddr0, r10, #0x3b0
jeq csr_pmpaddr1, r10, #0x3b1
jeq csr_pmpaddr2, r10, #0x3b2
jeq csr_pmpaddr3, r10, #0x3b3
jeq csr_pmpaddr4, r10, #0x3b4
jeq csr_pmpaddr5, r10, #0x3b5
jeq csr_pmpaddr6, r10, #0x3b6
jeq csr_pmpaddr7, r10, #0x3b7
jeq csr_pmpaddr8, r10, #0x3b8
jeq csr_pmpaddr9, r10, #0x3b9
jeq csr_pmpaddr10, r10, #0x3ba
jeq csr_pmpaddr11, r10, #0x3bb
jeq csr_pmpaddr12, r10, #0x3bc
jeq csr_pmpaddr13, r10, #0x3bd
jeq csr_pmpaddr14, r10, #0x3be
jeq csr_pmpaddr15, r10, #0x3bf
jump trap_udf

csr_mstatus:
add r5, cpr2t8, #0
ldi s3, put_mstatus
jump sys_store
put_mstatus:
add cpr2t8, r2, #0
jump op_wb
csr_mie:
add r5, cpr3t9, #0
ldi s3, put_mie
jump sys_store
put_mie:
add cpr3t9, r2, #0
jump op_wb
csr_mtvec:
add r5, cpr3t8, #0
ldi s3, put_mtvec
jump sys_store
put_mtvec:
add cpr3t8, r2, #0
jump op_wb
csr_mepc:
add r5, cpr0t9, #0
ldi s3, put_mepc
jump sys_store
put_mepc:
add cpr0t9, r2, #0
jump op_wb
csr_mcause:
add r5, cpr1t9, #0
ldi s3, put_mcause
jump sys_store
put_mcause:
add cpr1t9, r2, #0
jump op_wb
csr_mtval:
add r5, cpr2t9, #0
ldi s3, put_mtval
jump sys_store
put_mtval:
add cpr2t9, r2, #0
jump op_wb
csr_mscratch:
add r5, cpr1t10, #0
ldi s3, put_mscratch
jump sys_store
put_mscratch:
add cpr1t10, r2, #0
jump op_wb
csr_mip:
add r5, cpr0t10, #0
ldi s3, put_mip
jump sys_store
put_mip:
add cpr0t10, r2, #0
jump op_wb
csr_medeleg:
add r5, cpr2t10, #0
ldi s3, put_medeleg
jump sys_store
put_medeleg:
add cpr2t10, r2, #0
jump op_wb
csr_mideleg:
add r5, cpr3t10, #0
ldi s3, put_mideleg
jump sys_store
put_mideleg:
add cpr3t10, r2, #0
jump op_wb
csr_pmpcfg0:
add r5, cpr2t11, #0
ldi s3, put_pmpcfg0
jump sys_store
put_pmpcfg0:
add cpr2t11, r2, #0
jump op_wb
csr_pmpcfg1:
add r5, cpr3t11, #0
ldi s3, put_pmpcfg1
jump sys_store
put_pmpcfg1:
add cpr3t11, r2, #0
jump op_wb
csr_pmpaddr0:
add r5, cpr0t12, #0
ldi s3, put_pmpaddr0
jump sys_store
put_pmpaddr0:
add cpr0t12, r2, #0
jump op_wb
csr_pmpaddr1:
add r5, cpr1t12, #0
ldi s3, put_pmpaddr1
jump sys_store
put_pmpaddr1:
add cpr1t12, r2, #0
jump op_wb
csr_pmpaddr2:
add r5, cpr2t12, #0
ldi s3, put_pmpaddr2
jump sys_store
put_pmpaddr2:
add cpr2t12, r2, #0
jump op_wb
csr_pmpaddr3:
add r5, cpr3t12, #0
ldi s3, put_pmpaddr3
jump sys_store
put_pmpaddr3:
add cpr3t12, r2, #0
jump op_wb
csr_pmpaddr4:
add r5, cpr0t13, #0
ldi s3, put_pmpaddr4
jump sys_store
put_pmpaddr4:
add cpr0t13, r2, #0
jump op_wb
csr_pmpaddr5:
add r5, cpr1t13, #0
ldi s3, put_pmpaddr5
jump sys_store
put_pmpaddr5:
add cpr1t13, r2, #0
jump op_wb
csr_pmpaddr6:
add r5, cpr2t13, #0
ldi s3, put_pmpaddr6
jump sys_store
put_pmpaddr6:
add cpr2t13, r2, #0
jump op_wb
csr_pmpaddr7:
add r5, cpr3t13, #0
ldi s3, put_pmpaddr7
jump sys_store
put_pmpaddr7:
add cpr3t13, r2, #0
jump op_wb
csr_pmpaddr8:
add r5, cpr0t14, #0
ldi s3, put_pmpaddr8
jump sys_store
put_pmpaddr8:
add cpr0t14, r2, #0
jump op_wb
csr_pmpaddr9:
add r5, cpr1t14, #0
ldi s3, put_pmpaddr9
jump sys_store
put_pmpaddr9:
add cpr1t14, r2, #0
jump op_wb
csr_pmpaddr10:
add r5, cpr2t14, #0
ldi s3, put_pmpaddr10
jump sys_store
put_pmpaddr10:
add cpr2t14, r2, #0
jump op_wb
csr_pmpaddr11:
add r5, cpr3t14, #0
ldi s3, put_pmpaddr11
jump sys_store
put_pmpaddr11:
add cpr3t14, r2, #0
jump op_wb
csr_pmpaddr12:
add r5, cpr0t15, #0
ldi s3, put_pmpaddr12
jump sys_store
put_pmpaddr12:
add cpr0t15, r2, #0
jump op_wb
csr_pmpaddr13:
add r5, cpr1t15, #0
ldi s3, put_pmpaddr13
jump sys_store
put_pmpaddr13:
add cpr1t15, r2, #0
jump op_wb
csr_pmpaddr14:
add r5, cpr2t15, #0
ldi s3, put_pmpaddr14
jump sys_store
put_pmpaddr14:
add cpr2t15, r2, #0
jump op_wb
csr_pmpaddr15:
add r5, cpr3t15, #0
ldi s3, put_pmpaddr15
jump sys_store
put_pmpaddr15:
add cpr3t15, r2, #0
jump op_wb

sys_store:
jeq sys_wbskip, r6, #0
and r0, r9, #0x3
jeq sys_nw, r0, #0x1
jeq sys_ns, r0, #0x2
ldi s0, #0xffffffff
xor r2, r7, s0
and r2, r5, r2
jeq s3, r0, r0
sys_nw:
add r2, r7, #0
jeq s3, r0, r0
sys_ns:
orr r2, r5, r7
jeq s3, r0, r0
sys_wbskip:
jump op_wb

csr_misa:
ldi r5, #0x40001101
jne trap_udf, r6, #0
jump op_wb
csr_zero:
ldi r5, #0
jne trap_udf, r6, #0
jump op_wb
csr_time:
ldi r2, #0x10008048
read r5, [r2]
jne trap_udf, r6, #0
jump op_wb
csr_cycle:
ldi r2, #0x10008048
read r5, [r2]
jne trap_udf, r6, #0
jump op_wb
csr_instret:
ldi r2, #0x10008048
read r5, [r2]
jne trap_udf, r6, #0
jump op_wb
csr_mcycle:
ldi r2, #0x10008048
read r5, [r2]
jump op_wb
csr_mcycleh:
ldi r5, #0
jump op_wb
csr_minstret:
ldi r2, #0x10008048
read r5, [r2]
jump op_wb
csr_minstreth:
ldi r5, #0
jump op_wb

tmr_check:
ldi r2, #0x10008048
read r0, [r2]
add r2, cpr0t11, #0
jge tmr_due, r0, r2
add r0, cpr0t10, #0
ldi s0, #0xffffff7f
and r0, r0, s0
add cpr0t10, r0, #0
jump fetch
tmr_due:
add r0, cpr0t10, #0
and r0, r0, #0x80
jeq tmr_newdue, r0, #0
jump tmr_gate
tmr_newdue:
add r0, cpr0t10, #0
orr r0, r0, #0x80
add cpr0t10, r0, #0
tmr_gate:
add r0, cpr3t9, #0
and r0, r0, #0x80
jeq tmr_resume, r0, #0
add r0, cpr2t8, #0
and r0, r0, #0x8
jeq tmr_resume, r0, #0
ldi r5, #0x80000007
jump trap_enter
tmr_resume:
jump fetch

tbl_rs1:
ldi r7, #0
jeq s3, r0, r0
add r7, cpr1t0, #0
jeq s3, r0, r0
add r7, cpr2t0, #0
jeq s3, r0, r0
add r7, cpr3t0, #0
jeq s3, r0, r0
add r7, cpr0t1, #0
jeq s3, r0, r0
add r7, cpr1t1, #0
jeq s3, r0, r0
add r7, cpr2t1, #0
jeq s3, r0, r0
add r7, cpr3t1, #0
jeq s3, r0, r0
add r7, cpr0t2, #0
jeq s3, r0, r0
add r7, cpr1t2, #0
jeq s3, r0, r0
add r7, cpr2t2, #0
jeq s3, r0, r0
add r7, cpr3t2, #0
jeq s3, r0, r0
add r7, cpr0t3, #0
jeq s3, r0, r0
add r7, cpr1t3, #0
jeq s3, r0, r0
add r7, cpr2t3, #0
jeq s3, r0, r0
add r7, cpr3t3, #0
jeq s3, r0, r0
add r7, cpr0t4, #0
jeq s3, r0, r0
add r7, cpr1t4, #0
jeq s3, r0, r0
add r7, cpr2t4, #0
jeq s3, r0, r0
add r7, cpr3t4, #0
jeq s3, r0, r0
add r7, cpr0t5, #0
jeq s3, r0, r0
add r7, cpr1t5, #0
jeq s3, r0, r0
add r7, cpr2t5, #0
jeq s3, r0, r0
add r7, cpr3t5, #0
jeq s3, r0, r0
add r7, cpr0t6, #0
jeq s3, r0, r0
add r7, cpr1t6, #0
jeq s3, r0, r0
add r7, cpr2t6, #0
jeq s3, r0, r0
add r7, cpr3t6, #0
jeq s3, r0, r0
add r7, cpr0t7, #0
jeq s3, r0, r0
add r7, cpr1t7, #0
jeq s3, r0, r0
add r7, cpr2t7, #0
jeq s3, r0, r0
add r7, cpr3t7, #0
jeq s3, r0, r0
tbl_rs2:
ldi r6, #0
jeq s3, r0, r0
add r6, cpr1t0, #0
jeq s3, r0, r0
add r6, cpr2t0, #0
jeq s3, r0, r0
add r6, cpr3t0, #0
jeq s3, r0, r0
add r6, cpr0t1, #0
jeq s3, r0, r0
add r6, cpr1t1, #0
jeq s3, r0, r0
add r6, cpr2t1, #0
jeq s3, r0, r0
add r6, cpr3t1, #0
jeq s3, r0, r0
add r6, cpr0t2, #0
jeq s3, r0, r0
add r6, cpr1t2, #0
jeq s3, r0, r0
add r6, cpr2t2, #0
jeq s3, r0, r0
add r6, cpr3t2, #0
jeq s3, r0, r0
add r6, cpr0t3, #0
jeq s3, r0, r0
add r6, cpr1t3, #0
jeq s3, r0, r0
add r6, cpr2t3, #0
jeq s3, r0, r0
add r6, cpr3t3, #0
jeq s3, r0, r0
add r6, cpr0t4, #0
jeq s3, r0, r0
add r6, cpr1t4, #0
jeq s3, r0, r0
add r6, cpr2t4, #0
jeq s3, r0, r0
add r6, cpr3t4, #0
jeq s3, r0, r0
add r6, cpr0t5, #0
jeq s3, r0, r0
add r6, cpr1t5, #0
jeq s3, r0, r0
add r6, cpr2t5, #0
jeq s3, r0, r0
add r6, cpr3t5, #0
jeq s3, r0, r0
add r6, cpr0t6, #0
jeq s3, r0, r0
add r6, cpr1t6, #0
jeq s3, r0, r0
add r6, cpr2t6, #0
jeq s3, r0, r0
add r6, cpr3t6, #0
jeq s3, r0, r0
add r6, cpr0t7, #0
jeq s3, r0, r0
add r6, cpr1t7, #0
jeq s3, r0, r0
add r6, cpr2t7, #0
jeq s3, r0, r0
add r6, cpr3t7, #0
jeq s3, r0, r0
tbl_rd:
jump +8
jeq s3, r0, r0
add cpr1t0, r5, #0
jeq s3, r0, r0
add cpr2t0, r5, #0
jeq s3, r0, r0
add cpr3t0, r5, #0
jeq s3, r0, r0
add cpr0t1, r5, #0
jeq s3, r0, r0
add cpr1t1, r5, #0
jeq s3, r0, r0
add cpr2t1, r5, #0
jeq s3, r0, r0
add cpr3t1, r5, #0
jeq s3, r0, r0
add cpr0t2, r5, #0
jeq s3, r0, r0
add cpr1t2, r5, #0
jeq s3, r0, r0
add cpr2t2, r5, #0
jeq s3, r0, r0
add cpr3t2, r5, #0
jeq s3, r0, r0
add cpr0t3, r5, #0
jeq s3, r0, r0
add cpr1t3, r5, #0
jeq s3, r0, r0
add cpr2t3, r5, #0
jeq s3, r0, r0
add cpr3t3, r5, #0
jeq s3, r0, r0
add cpr0t4, r5, #0
jeq s3, r0, r0
add cpr1t4, r5, #0
jeq s3, r0, r0
add cpr2t4, r5, #0
jeq s3, r0, r0
add cpr3t4, r5, #0
jeq s3, r0, r0
add cpr0t5, r5, #0
jeq s3, r0, r0
add cpr1t5, r5, #0
jeq s3, r0, r0
add cpr2t5, r5, #0
jeq s3, r0, r0
add cpr3t5, r5, #0
jeq s3, r0, r0
add cpr0t6, r5, #0
jeq s3, r0, r0
add cpr1t6, r5, #0
jeq s3, r0, r0
add cpr2t6, r5, #0
jeq s3, r0, r0
add cpr3t6, r5, #0
jeq s3, r0, r0
add cpr0t7, r5, #0
jeq s3, r0, r0
add cpr1t7, r5, #0
jeq s3, r0, r0
add cpr2t7, r5, #0
jeq s3, r0, r0
add cpr3t7, r5, #0
jeq s3, r0, r0

end:
jump +0

// fault handler
trap_udf:
ldi r2, #0x11002000
ldi r8, #0x11002014
ldi r0, #'u'
ldi s3, tr1
jump trap_putc
tr1:
ldi r0, #'d'
ldi s3, tr2
jump trap_putc
tr2:
ldi r0, #'f'
ldi s3, tr3
jump trap_putc
tr3:
ldi r0, #':'
ldi s3, tr4
jump trap_putc
tr4:
ldi r3, #28
hex_loop:
lsr r0, r15, r3
and r0, r0, #0xf
jge hex_letter, r0, #10
add r0, r0, #0x30
jeq hex_emit, r0, r0
hex_letter:
add r0, r0, #0x57
hex_emit:
ldi s3, hex_next
jump trap_putc
hex_next:
jeq hex_done, r3, #0
sub r3, r3, #4
jump hex_loop
hex_done:
jump +0
trap_putc:
read r4, [r8]
and r4, r4, #0x20
jne trap_putc_go, r4, #0
jump trap_putc
trap_putc_go:
write [r2], r0
jeq s3, r0, r0

