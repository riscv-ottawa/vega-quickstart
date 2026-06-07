#ifndef _PERIPHERALS_H_
#define _PERIPHERALS_H_

#include <stdint.h>
#include "fsl_common.h"

/*
 * Interrupt routing on the RV32M1 RI5CY core.
 *
 *   peripheral ---> INTMUX0 channel N ---> EVENT_UNIT line ---> core trap
 *
 * INTMUX0 has 8 channels; their outputs are wired to RI5CY external
 * interrupt lines 24-31 (matching INTMUX0_0_IRQn .. INTMUX0_7_IRQn in
 * the SDK). LPTMR0's peripheral IRQ (LPTMR0_IRQn) is one of the sources
 * on channel 0; its bit position inside CHn_IER_31_0 is
 * (LPTMR0_IRQn - 32) per the SDK's INTMUX_EnableInterrupt convention.
 */

#define LPTMR0_INTMUX_CHANNEL  0u
#define LPTMR0_INTMUX_BIT      ((uint32_t)LPTMR0_IRQn - 32u)

/* Tick rate of the LPTMR-driven heartbeat, in Hz. */
#define TICK_RATE_HZ 100u

void install_trap_handler(void);

void lptmr_init_hz(uint32_t hz);
void intmux0_enable(uint32_t channel, uint32_t source_bit);
void eventunit_enable(uint32_t line);
void set_mstatus_mie(void);

/* Implemented by trap.c; called from trap_entry via trap_handler. */
void irq_dispatch(uint32_t line);

/* Defined per-app. Called from the INTMUX0 channel-0 dispatcher when
 * the LPTMR0 source bit is pending. */
void lptmr0_isr(void);

#endif /* _PERIPHERALS_H_ */
