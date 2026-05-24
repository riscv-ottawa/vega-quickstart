#include "fsl_common.h"
#include "fsl_port.h"
#include "fsl_msmc.h"
#include "fsl_lpuart.h"
#include "board.h"

extern uint32_t SystemCoreClock;

/* Pin mux: route PTC7/PTC8 to LPUART0 RX/TX. */
void BOARD_InitPins(void)
{
    CLOCK_EnableClock(kCLOCK_PortC);

    PORT_SetPinMux(PORTC, 7U, kPORT_MuxAlt3);   /* PTC7 - LPUART0_RX */
    PORT_SetPinMux(PORTC, 8U, kPORT_MuxAlt3);   /* PTC8 - LPUART0_TX */
}

/*
 * Clock setup: configure the system to run at 48 MHz from the FIRC.
 * Identical to apps/blinky/board.c; see that file for the commentary.
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

/*
 * UART setup: drive LPUART0 directly with LPUART_Init at 115200 8N1.
 * No debug console wrapper - main.c reads and writes bytes itself.
 */
void BOARD_InitUart(void)
{
    CLOCK_SetIpSrc(kCLOCK_Lpuart0, kCLOCK_IpSrcFircAsync);

    lpuart_config_t cfg;
    LPUART_GetDefaultConfig(&cfg);
    cfg.baudRate_Bps = BOARD_DEBUG_UART_BAUD;
    cfg.enableTx = true;
    cfg.enableRx = true;

    // NOTE: Hi there, using CLOCK_GetIpFreq() causes crazy problems when run in Renode.
    // I haven't been able to figure out why, so I just hardcoded the clock freq. instead.
    //
    // P.S. good on you for actually reading the source code, here's a cookie as a reward 🍪
    //
    // LPUART_Init(BOARD_DEBUG_UART, &cfg, CLOCK_GetIpFreq(kCLOCK_Lpuart0));
    LPUART_Init(BOARD_DEBUG_UART, &cfg, 48000000U);
}
