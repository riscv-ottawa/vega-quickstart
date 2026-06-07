#include "fsl_common.h"
#include "fsl_port.h"
#include "fsl_msmc.h"
#include "fsl_debug_console.h"
#include "board.h"

extern uint32_t SystemCoreClock;

/* Pin mux: configure GPIO and UART pins */
void BOARD_InitPins(void)
{
    CLOCK_EnableClock(kCLOCK_PortA);
    CLOCK_EnableClock(kCLOCK_PortC);

    PORT_SetPinMux(PORTA, BOARD_LED_GPIO_PIN, kPORT_MuxAsGpio);  /* PTA24 - Red LED */
    PORT_SetPinMux(PORTC, 7U, kPORT_MuxAlt3);                    /* PTC7  - LPUART0_RX */
    PORT_SetPinMux(PORTC, 8U, kPORT_MuxAlt3);                    /* PTC8  - LPUART0_TX */
}

/*
 * Clock setup: configure the system to run at 48 MHz from the Fast IRC (FIRC).
 *
 * Out of reset the CPU already runs from FIRC, but we can't reconfigure FIRC
 * while it's the active clock source. So we temporarily switch to SIRC (8 MHz
 * slow IRC), reconfigure FIRC to our desired settings, then switch back.
 */
void BOARD_BootClockRUN(void)
{
    scg_sys_clk_config_t curConfig;

    /* Step 1: Enable SIRC so we have a safe clock to switch to */
    const scg_sirc_config_t sircConfig = {
        .enableMode = kSCG_SircEnable,
        .div1 = kSCG_AsyncClkDisable,
        .div2 = kSCG_AsyncClkDivBy2,
        .range = kSCG_SircRangeHigh,
    };
    CLOCK_InitSirc(&sircConfig);

    /* Step 2: Switch system clock to SIRC */
    scg_sys_clk_config_t safeConfig = {
        .divSlow = kSCG_SysClkDivBy4,
        .divCore = kSCG_SysClkDivBy1,
        .src = kSCG_SysClkSrcSirc,
    };
    CLOCK_SetRunModeSysClkConfig(&safeConfig);
    do {
        CLOCK_GetCurSysClkConfig(&curConfig);
    } while (curConfig.src != kSCG_SysClkSrcSirc);

    /* Step 3: Now safe to reconfigure FIRC (48 MHz) */
    const scg_firc_config_t fircConfig = {
        .enableMode = kSCG_FircEnable,
        .div1 = kSCG_AsyncClkDivBy1,
        .div2 = kSCG_AsyncClkDivBy1,
        .div3 = kSCG_AsyncClkDivBy1,
        .range = kSCG_FircRange48M,
        .trimConfig = NULL,
    };
    CLOCK_InitFirc(&fircConfig);

    /* Step 4: Switch system clock back to FIRC */
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

    /* Re-init SIRC with final settings (enable in low-power mode) */
    const scg_sirc_config_t sircFinal = {
        .enableMode = kSCG_SircEnable | kSCG_SircEnableInLowPower,
        .div1 = kSCG_AsyncClkDisable,
        .div2 = kSCG_AsyncClkDisable,
        .div3 = kSCG_AsyncClkDivBy1,
        .range = kSCG_SircRangeHigh,
    };
    CLOCK_InitSirc(&sircFinal);

    /* Disable LPFLL (not needed at 48 MHz) */
    const scg_lpfll_config_t lpfllConfig = {
        .enableMode = 0U,
        .div1 = kSCG_AsyncClkDivBy1,
        .div2 = kSCG_AsyncClkDisable,
        .div3 = kSCG_AsyncClkDisable,
        .range = kSCG_LpFllRange48M,
        .trimConfig = NULL,
    };
    CLOCK_InitLpFll(&lpfllConfig);

    SystemCoreClock = 48000000U;
}

/* Debug console: LPUART0 at 115200 baud, clocked from FIRC async */
void BOARD_InitDebugConsole(void)
{
    CLOCK_SetIpSrc(kCLOCK_Lpuart0, kCLOCK_IpSrcFircAsync);
    DbgConsole_Init((uint32_t)LPUART0, 115200U,
                    DEBUG_CONSOLE_DEVICE_TYPE_LPUART,
                    CLOCK_GetIpFreq(kCLOCK_Lpuart0));
}
