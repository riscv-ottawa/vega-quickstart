#ifndef _BOARD_H_
#define _BOARD_H_

#include "fsl_gpio.h"

/*
 * The on-board RGB LED is wired entirely to GPIOA:
 *   red   = PTA24
 *   green = PTA23
 *   blue  = PTA22
 */
#define BOARD_LED_GPIO        GPIOA
#define BOARD_LED_RED_PIN     24U
#define BOARD_LED_GREEN_PIN   23U
#define BOARD_LED_BLUE_PIN    22U

void BOARD_InitPins(void);
void BOARD_BootClockRUN(void);
void BOARD_InitDebugConsole(void);

#endif /* _BOARD_H_ */
