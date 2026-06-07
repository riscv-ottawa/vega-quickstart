/*
 * Blinky, but better.
 *
 * Same visible behaviour as apps/blinky - the red LED toggles at 1 Hz -
 * but the timing is entirely owned by an LPTMR ISR. main() runs through
 * init and then drops into a wfi loop; the core sleeps between events.
 *
 * See the "Blinky, but better!" section in the vega-training book.
 */
#include "board.h"
#include "fsl_common.h"
#include "fsl_debug_console.h"
#include "fsl_gpio.h"
#include "peripherals.h"
#include <stdint.h>

#define BLINK_HZ 2u /* two toggles per second = 1 Hz visible blink */

volatile uint32_t g_blinks;

static void led_init(void) {
  gpio_pin_config_t cfg = {kGPIO_DigitalOutput, 0};
  GPIO_PinInit(BOARD_LED_GPIO, BOARD_LED_GPIO_PIN, &cfg);
}

static inline void led_toggle(void) {
  BOARD_LED_GPIO->PTOR = 1u << BOARD_LED_GPIO_PIN;
}

void lptmr0_isr(void) {
  /*
   * Ack the compare flag (write-1-to-clear of TCF).
   * Note, Renode's LowPower_Timer model uses a descending
   * counter that gets stuck at 0 after firing, so we
   * also write CNR to reload the counter to the limit. CNR is
   * read-only on real hardware so the second write is a no-op there.
   */
  LPTMR0->CSR |= LPTMR_CSR_TCF_MASK;
  LPTMR0->CNR = LPTMR0->CMR;
  led_toggle();
  g_blinks++;
}

int main(void) {
  BOARD_InitPins();
  BOARD_BootClockRUN();
  BOARD_InitDebugConsole();
  led_init();

  install_trap_handler();
  intmux0_enable(LPTMR0_INTMUX_CHANNEL, LPTMR0_INTMUX_BIT);
  eventunit_enable(INTMUX0_0_IRQn);
  set_mstatus_mie();
  /* Start the LPTMR last. Renode's NXP_INTMUX appears to be edge-
   * sensitive, so the channel mask must be in place before the
   * first compare-match fires. */
  lptmr_init_hz(BLINK_HZ);

  PRINTF("\r\nblinky, but better. main going to sleep.\r\n");
  while(1) {
    __asm__ volatile("wfi");
  }
  return 0; /* unreachable */
}
