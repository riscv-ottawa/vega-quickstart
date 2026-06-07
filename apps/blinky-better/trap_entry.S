/*
 * Trap entry for the RV32M1 RI5CY core.
 *
 * RI5CY is NOT a standard RISC-V "direct mode" core. mtvec is the base
 * of a hard-wired *vector table*: on any trap the core jumps to
 * mtvec_base + a fixed offset that depends on the trap. The offsets,
 * matching the SDK's startup_RV32M1_ri5cy.S .vectors table, are:
 *
 *     0x00 .. 0x7C   interrupt lines 0..31   (mtvec_base + line*4)
 *     0x80           reset
 *     0x84           illegal instruction
 *     0x88           ecall
 *     0x8C           LSU (load/store) error
 *
 * mtvec must point at a real table. Every entry just jumps into the
 * common save/dispatch body below; trap_handler reads mcause to tell
 * interrupts from exceptions. .option norvc keeps every `j` a full
 * 4 bytes so the slot spacing stays exactly 4 (a compressed c.j would
 * shift every later offset).
 *
 * The core saves no general-purpose registers and does not switch
 * stacks, so the body spills the caller-saved set, calls into C,
 * restores, and mret. Callee-saved regs (s0-s11, sp) are handled by the
 * compiler across the jal into trap_handler.
 *
 */

    .section .text.trap_entry, "ax"
    /*
     * RI5CY constrains the mtvec BASE: the low bits are not all writable,
     * so the vector table must be aligned (the SDK puts __VECTOR_TABLE in
     * its own 256-byte region at 0x000FFF00).
     */
    .align   8
    .option  norvc
    .global  trap_entry
trap_entry:
    .rept 32
    j   trap_body          /* 0x00..0x7C: interrupt lines 0..31 */
    .endr
    j   trap_body          /* 0x80: reset                       */
    j   trap_body          /* 0x84: illegal instruction         */
    j   trap_body          /* 0x88: ecall                       */
    j   trap_body          /* 0x8C: LSU error                   */
    .option  rvc

trap_body:
    addi    sp, sp, -64

    sw      ra,  0(sp)
    sw      t0,  4(sp)
    sw      t1,  8(sp)
    sw      t2, 12(sp)
    sw      a0, 16(sp)
    sw      a1, 20(sp)
    sw      a2, 24(sp)
    sw      a3, 28(sp)
    sw      a4, 32(sp)
    sw      a5, 36(sp)
    sw      a6, 40(sp)
    sw      a7, 44(sp)
    sw      t3, 48(sp)
    sw      t4, 52(sp)
    sw      t5, 56(sp)
    sw      t6, 60(sp)

    jal     trap_handler

    lw      ra,  0(sp)
    lw      t0,  4(sp)
    lw      t1,  8(sp)
    lw      t2, 12(sp)
    lw      a0, 16(sp)
    lw      a1, 20(sp)
    lw      a2, 24(sp)
    lw      a3, 28(sp)
    lw      a4, 32(sp)
    lw      a5, 36(sp)
    lw      a6, 40(sp)
    lw      a7, 44(sp)
    lw      t3, 48(sp)
    lw      t4, 52(sp)
    lw      t5, 56(sp)
    lw      t6, 60(sp)

    addi    sp, sp, 64
    mret
