#ifndef _BOARD_H_
#define _BOARD_H_

#include "fsl_lpuart.h"

#define BOARD_DEBUG_UART        LPUART0
#define BOARD_DEBUG_UART_BAUD   115200U

void BOARD_InitPins(void);
void BOARD_BootClockRUN(void);
void BOARD_InitUart(void);

#endif /* _BOARD_H_ */
