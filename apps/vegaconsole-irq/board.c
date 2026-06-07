#include "fsl_common.h"
#include "fsl_port.h"
#include "fsl_msmc.h"
#include "fsl_debug_console.h"
#include "board.h"

extern uint32_t SystemCoreClock;

/*
 * Pin mux: all three RGB-LED pins as GPIO, plus the LPUART0 RX/TX pins
 * for the serial console. The LED pins are PTA22/23/24, the UART pins
 * are PTC7/PTC8.
 */
void BOARD_InitPins(void)
{
    CLOCK_EnableClock(kCLOCK_PortA);
    CLOCK_EnableClock(kCLOCK_PortC);

    PORT_SetPinMux(PORTA, BOARD_LED_RED_PIN,   kPORT_MuxAsGpio);
    PORT_SetPinMux(PORTA, BOARD_LED_GREEN_PIN, kPORT_MuxAsGpio);
    PORT_SetPinMux(PORTA, BOARD_LED_BLUE_PIN,  kPORT_MuxAsGpio);

    PORT_SetPinMux(PORTC, 7U, kPORT_MuxAlt3);   /* PTC7 - LPUART0_RX */
    PORT_SetPinMux(PORTC, 8U, kPORT_MuxAlt3);   /* PTC8 - LPUART0_TX */
}

/*
 * Clock setup: 48 MHz from the FIRC. Identical to apps/blinky/board.c;
 * see that file for more info.
 */
void BOARD_BootClockRUN(void)
{
    scg_sys_clk_config_t curConfig;

    const scg_sirc_config_t sircConfig = {
        .enableMode = kSCG_SircEnable,
        .div1 = kSCG_AsyncClkDisable,
        .div2 = kSCG_AsyncClkDivBy2,
        .range = kSCG_SircRangeHigh,
    };
    CLOCK_InitSirc(&sircConfig);

    scg_sys_clk_config_t safeConfig = {
        .divSlow = kSCG_SysClkDivBy4,
        .divCore = kSCG_SysClkDivBy1,
        .src = kSCG_SysClkSrcSirc,
    };
    CLOCK_SetRunModeSysClkConfig(&safeConfig);
    do {
        CLOCK_GetCurSysClkConfig(&curConfig);
    } while (curConfig.src != kSCG_SysClkSrcSirc);

    const scg_firc_config_t fircConfig = {
        .enableMode = kSCG_FircEnable,
        .div1 = kSCG_AsyncClkDivBy1,
        .div2 = kSCG_AsyncClkDivBy1,
        .div3 = kSCG_AsyncClkDivBy1,
        .range = kSCG_FircRange48M,
        .trimConfig = NULL,
    };
    CLOCK_InitFirc(&fircConfig);

    scg_sys_clk_config_t runConfig = {
        .divSlow = kSCG_SysClkDivBy2,
        .divBus  = kSCG_SysClkDivBy1,
        .divExt  = kSCG_SysClkDivBy1,
        .divCore = kSCG_SysClkDivBy1,
        .src     = kSCG_SysClkSrcFirc,
    };
    CLOCK_SetRunModeSysClkConfig(&runConfig);
    do {
        CLOCK_GetCurSysClkConfig(&curConfig);
    } while (curConfig.src != kSCG_SysClkSrcFirc);

    SystemCoreClock = 48000000U;
}

void BOARD_InitDebugConsole(void)
{
    CLOCK_SetIpSrc(kCLOCK_Lpuart0, kCLOCK_IpSrcFircAsync);
    DbgConsole_Init((uint32_t)LPUART0, 115200U,
                    DEBUG_CONSOLE_DEVICE_TYPE_LPUART,
                    CLOCK_GetIpFreq(kCLOCK_Lpuart0));
}
