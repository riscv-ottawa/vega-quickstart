#include <stdint.h>
#include <stdbool.h>
#include "fsl_common.h"
#include "fsl_intmux.h"
#include "peripherals.h"

/*
 * Peripheral setup for the trap path: LPTMR0 as the periodic tick
 * source, INTMUX0 channel 0 as the fan-in, EVENT_UNIT as the final
 * gate before the core.
 *
 * See: "EVENT_UNIT and LPTMR" in the training book.
 */

/* ---- LPTMR0 ---- */

/*
 * Configure LPTMR0 to fire 'hz' compare-match interrupts per second.
 *
 * LPTMR's PSR.PCS field selects the clock source directly (no PCC mux
 * involved). PCS=1 picks the 1 kHz LPO, which is always on and needs
 * no other setup. PBYP=1 bypasses the prescaler so the counter ticks
 * at exactly 1 kHz; then CMR = 1000 / hz fires 'hz' matches per
 * second.
 */
void lptmr_init_hz(uint32_t hz)
{
    LPTMR0->CSR = 0u;                                /* disabled while configuring */
    LPTMR0->PSR = LPTMR_PSR_PCS(1)                   /* clock source: LPO 1 kHz   */
                | LPTMR_PSR_PBYP_MASK;               /* bypass the prescaler       */
    LPTMR0->CMR = 1000u / hz;
    LPTMR0->CNR = LPTMR0->CMR;                       /* seed counter (Renode only; CNR is RO on real HW) */
    LPTMR0->CSR = LPTMR_CSR_TIE_MASK | LPTMR_CSR_TEN_MASK;
}

/* ---- INTMUX0 ---- */

/*
 * The first call to intmux0_enable() does the one-time INTMUX setup:
 * enable INTMUX0's clock gate at the PCC, reset all 8 channels, and
 * enable each channel's output line at the EVENT_UNIT. Subsequent
 * calls just flip the requested source bit on the requested channel.
 */
void intmux0_enable(uint32_t channel, uint32_t source_bit)
{
    static bool initialized;
    if (!initialized) {
        INTMUX_Init(INTMUX0);
        initialized = true;
    }
    INTMUX0->CHANNEL[channel].CHn_IER_31_0 |= (1u << source_bit);
}

/* ---- EVENT_UNIT ---- */

/*
 * Note: INTMUX_Init above already enables all 8 INTMUX channel output
 * lines at the EVENT_UNIT. Calling this for one of those lines is
 * redundant but harmless. For non-INTMUX interrupts (peripherals
 * wired directly to the core, IRQ 0-23), this is the only enable
 * step at the EVENT_UNIT.
 */
void eventunit_enable(uint32_t line)
{
    EVENT_UNIT->INTPTEN |= (1u << line);
}

/* ---- mstatus.MIE (global interrupt enable) ---- */

void set_mstatus_mie(void)
{
    __asm__ volatile ("csrrs zero, mstatus, %0" :: "r"(1u << 3));
}

/* ---- IRQ dispatch ---- */

static void intmux0_ch0_dispatch(void)
{
    uint32_t pending = INTMUX0->CHANNEL[0].CHn_IPR_31_0;
    if (pending & (1u << LPTMR0_INTMUX_BIT)) {
        lptmr0_isr();
    }
    /* Other peripherals on this channel would be checked here. */
}

void irq_dispatch(uint32_t line)
{
    switch (line) {
    case INTMUX0_0_IRQn:
        intmux0_ch0_dispatch();
        break;
    default:
        /* Unhandled line. In a production handler you'd log or assert;
         * here we just return and let the program continue. */
        break;
    }

    /*
     * Acknowledge the line at the EVENT_UNIT. The chain is
     * LPTMR -> INTMUX0 -> EVENT_UNIT -> core; clearing only the leaf flag
     * (LPTMR's TCF, done in the ISR above) is not enough since the EVENT
     * unit latches the request and keeps re-asserting it to the core.
     */
    EVENT_UNIT->INTPTPENDCLEAR = (1u << line);
}
