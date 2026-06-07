#ifndef _BOARD_H_
#define _BOARD_H_

#include "fsl_gpio.h"

/* Red LED: GPIOA pin 24 (same as apps/blinky). */
#define BOARD_LED_GPIO     GPIOA
#define BOARD_LED_GPIO_PIN 24U

void BOARD_InitPins(void);
void BOARD_BootClockRUN(void);
void BOARD_InitDebugConsole(void);

#endif /* _BOARD_H_ */
