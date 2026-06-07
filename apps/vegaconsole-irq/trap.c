#include <stdint.h>
#include "fsl_debug_console.h"
#include "peripherals.h"

/*
 * Top-level trap dispatcher. Called from trap_entry (in trap.S) after
 * the caller-saved registers have been spilled.
 *
 * mcause's top bit distinguishes interrupts (1) from exceptions (0).
 * The low bits identify the specific cause. See chapter 3.1.3 of the
 * RV32M1 reference manual for the cause codes.
 *
 */
extern void trap_entry(void);

static void exception_handler(uint32_t cause, uint32_t epc)
{
    PRINTF("\r\ntrap! cause=%u epc=0x%08x\r\n", (unsigned)cause, (unsigned)epc);

    /*
     * Step past the offending instruction. Compressed (RVC) instructions
     * are 2 bytes; everything else is 4. Check the low two bits of the
     * instruction itself to decide.
     */
    uint16_t insn = *(volatile uint16_t *)epc;
    uint32_t step = ((insn & 0x3u) == 0x3u) ? 4u : 2u;
    __asm__ volatile ("csrw mepc, %0" :: "r"(epc + step));
}

void trap_handler(void)
{
    uint32_t cause, epc;
    __asm__ volatile ("csrr %0, mcause" : "=r"(cause));
    __asm__ volatile ("csrr %0, mepc"   : "=r"(epc));

    if (cause & 0x80000000u) {
        irq_dispatch(cause & 0x7FFFFFFFu);
    } else {
        exception_handler(cause, epc);
    }
}

void install_trap_handler(void)
{
    /* RI5CY treats mtvec as the base of a vector table, not a single
     * direct-mode entry point: each trap lands at mtvec_base + a fixed
     * offset (interrupt line*4, illegal insn at 0x84, ...). trap_entry
     * is that table (see trap_entry.S); it is .align 2 so the low bits
     * are zero as the base field requires. */
    __asm__ volatile ("csrw mtvec, %0" :: "r"((uintptr_t)trap_entry));
}
