# Pick up the trap entry stub for this app. trap.S is not in $(APP_DIR)/*.c
# so it has to be added to ASM_SRCS explicitly.
ASM_SRCS += $(APP_DIR)/trap_entry.S

# fsl_intmux.c is not in the default SDK_SRCS; pull it in for the
# INTMUX_Init / INTMUX_ResetChannel helpers used in peripherals.c.
SDK_SRCS += $(DRIVER_DIR)/fsl_intmux.c
