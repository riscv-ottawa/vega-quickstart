#include "board.h"
#include "fsl_lpuart.h"
#include "fsl_debug_console.h"

/* Block until the TX data register is empty, then push one byte. */
static void uart_putc(char c)
{
    while (!(LPUART_GetStatusFlags(BOARD_DEBUG_UART) & kLPUART_TxDataRegEmptyFlag))
        ;
    LPUART_WriteByte(BOARD_DEBUG_UART, (uint8_t)c);
}

/* Block until a byte arrives, then return it. */
static char uart_getc(void)
{
    while (!(LPUART_GetStatusFlags(BOARD_DEBUG_UART) & kLPUART_RxDataRegFullFlag))
        ;
    return (char)LPUART_ReadByte(BOARD_DEBUG_UART);
}

static void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

int main(void)
{
    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitUart();
    CLOCK_SetIpSrc(kCLOCK_Lpuart0, kCLOCK_IpSrcFircAsync);
    DbgConsole_Init((uint32_t)LPUART0, 115200U,
                    DEBUG_CONSOLE_DEVICE_TYPE_LPUART,
                    CLOCK_GetIpFreq(kCLOCK_Lpuart0));

    uart_puts("\r\nhello-uart: type something for it to echo back capitalized.\r\n> ");
    for (;;)
    {
        char c = uart_getc();

        /* Translate Enter into CRLF so the terminal moves to a new line. */
        if (c == '\r')
        {
            uart_puts("\r\n> ");
            continue;
        }
        if (c >= 'a' && c <= 'z')
            c -= ('a' - 'A');

        uart_putc(c);
    }
}
