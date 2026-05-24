# Build system for VEGA RV32M1 training.
#
# Usage:
#   make <app>           builds the given application
#   make flash-<app>     build and flash <app> via OpenOCD
#   make sim-<app>       run <app> in Renode
#   make gdbserver       start OpenOCD as a GDB server (listens on :3333)
#   make gdb-<app>       connect GDB to a running gdbserver, loading <app>.elf
#   make clean           remove all build artifacts
#   make list            list available apps
#   make serial          open serial console
#
# A new app only needs a directory under apps/ with .c sources in it.
# To customize (e.g. pick a different -march, add/remove SDK drivers),
# drop an apps/<app>/config.mk that overrides any of the building-block
# variables (ARCH_FLAGS, APP_SRCS, SDK_SRCS, INCLUDES, DEFINES, ...).

SHELL := bash
.ONESHELL:
.DELETE_ON_ERROR:
MAKEFLAGS += --no-builtin-rules

APPS := $(patsubst apps/%/,%,$(wildcard apps/*/))

OPENOCD     ?= openocd
OPENOCD_CFG := support/openocd/openocd_rv32m1_vega_ri5cy.cfg

.PHONY: all list clean serial build flash gdb gdbserver
.PHONY: $(APPS) $(addprefix flash-,$(APPS)) $(addprefix sim-,$(APPS)) $(addprefix gdb-,$(APPS))

# ============================================================================
# Dispatcher: no APP set, route per-app targets into a sub-make with APP=<name>
# ============================================================================
ifeq ($(APP),)

all: $(APPS)

list:
	@echo $(APPS)

clean:
	rm -rf build

$(APPS):
	@$(MAKE) --no-print-directory APP=$@ build

$(addprefix flash-,$(APPS)): flash-%:
	@$(MAKE) --no-print-directory APP=$* flash

gdbserver:
	$(OPENOCD) -f $(OPENOCD_CFG) -c "init" -c "halt" -c "ri5cy_boot"

$(addprefix gdb-,$(APPS)): gdb-%:
	@$(MAKE) --no-print-directory APP=$* gdb

RENODE ?= renode

$(addprefix sim-,$(APPS)): sim-%: %
	$(RENODE) <(cat <<-'EOF'
		$$bin=@build/$*/$*.elf
		include @support/renode/vegaboard_ri5cy.resc
		start
	EOF
	)

SERIAL_DEV ?= $(firstword $(wildcard /dev/ttyACM* /dev/ttyUSB* /dev/cu.usbmodem*))

serial:
	@[ -n "$(SERIAL_DEV)" ] || { echo "No serial device found"; exit 1; }
	sudo minicom -D $(SERIAL_DEV) -b 115200

else
# ============================================================================
# Per-app build: APP=<name> is set, build that single app
# ============================================================================

# Toolchain
CROSS_COMPILE ?= riscv32-unknown-elf-
CC      := $(CROSS_COMPILE)gcc
AS      := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
SIZE    := $(CROSS_COMPILE)size
GDB     := $(CROSS_COMPILE)gdb

# Directories
APP_DIR    := apps/$(APP)
BUILD_DIR  := build/$(APP)
SDK_ROOT   ?= rv32m1-sdk
DEVICE_DIR := $(SDK_ROOT)/devices/RV32M1
DRIVER_DIR := $(DEVICE_DIR)/drivers
UTIL_DIR   := $(DEVICE_DIR)/utilities
BOARD_DIR  := $(SDK_ROOT)/boards/rv32m1_vega

LDSCRIPT    := support/sdk/RV32M1_ri5cy_flash.ld

ARCH_FLAGS := -march=rv32imc

# App sources: main.c first (if present), then the rest
APP_SRCS := $(wildcard $(APP_DIR)/main.c) \
            $(filter-out $(APP_DIR)/main.c,$(wildcard $(APP_DIR)/*.c))

SDK_SRCS := \
	$(DEVICE_DIR)/system_RV32M1_ri5cy.c \
	$(DRIVER_DIR)/fsl_common.c \
	$(DRIVER_DIR)/fsl_gpio.c \
	$(DRIVER_DIR)/fsl_clock.c \
	$(DRIVER_DIR)/fsl_msmc.c \
	$(DRIVER_DIR)/fsl_lpuart.c \
	$(UTIL_DIR)/fsl_debug_console.c \
	$(UTIL_DIR)/fsl_io.c \
	$(UTIL_DIR)/fsl_log.c \
	$(UTIL_DIR)/fsl_str.c

ASM_SRCS := $(DEVICE_DIR)/gcc/startup_RV32M1_ri5cy.S

INCLUDES := \
	-I$(APP_DIR) \
	-I$(DEVICE_DIR) \
	-I$(DRIVER_DIR) \
	-I$(UTIL_DIR) \
	-I$(SDK_ROOT)/RISCV \
	-I$(SDK_ROOT)/devices

DEFINES := \
	-DCPU_RV32M1_ri5cy \
	-D__STARTUP_CLEAR_BSS

# Per-app overrides land here, before final flags are assembled.
-include $(APP_DIR)/config.mk

CFLAGS := $(ARCH_FLAGS) $(DEFINES) $(INCLUDES) \
	-g -O0 -Wall \
	-fno-common -ffunction-sections -fdata-sections \
	-ffreestanding -fno-builtin \
	-std=gnu99 \
	-MMD -MP

ASFLAGS := $(ARCH_FLAGS) $(DEFINES) \
	-g -Wall \
	-fno-common -ffunction-sections -fdata-sections \
	-ffreestanding -fno-builtin

LDFLAGS := $(ARCH_FLAGS) \
	-T$(LDSCRIPT) \
	-ffreestanding -fno-builtin -nostdlib \
	-Xlinker --gc-sections \
	-Xlinker -static \
	-Xlinker -z -Xlinker muldefs

LDLIBS := -Wl,--start-group -lm -lc -lgcc -lnosys -Wl,--end-group

# Flatten sources to basenames in the build dir; VPATH resolves them back.
C_SRCS   := $(APP_SRCS) $(SDK_SRCS)
C_OBJS   := $(addprefix $(BUILD_DIR)/,$(notdir $(C_SRCS:.c=.o)))
ASM_OBJS := $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SRCS:.S=.o)))
OBJS     := $(C_OBJS) $(ASM_OBJS)

VPATH := $(sort $(dir $(C_SRCS) $(ASM_SRCS)))

TARGET := $(BUILD_DIR)/$(APP)

build: $(TARGET).elf $(TARGET).bin $(TARGET).hex
	@$(SIZE) $(TARGET).elf

$(BUILD_DIR):
	@mkdir -p $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: %.S | $(BUILD_DIR)
	$(AS) $(ASFLAGS) -c -o $@ $<

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

flash: $(TARGET).elf
	$(OPENOCD) -f $(OPENOCD_CFG) \
		-c "init" \
		-c "halt" \
		-c "ri5cy_boot" \
		-c "flash write_image erase $(TARGET).elf" \
		-c "reset run" \
		-c "exit"

gdb: $(TARGET).elf
	$(GDB) $< -ex "target remote :3333"

-include $(C_OBJS:.o=.d)

endif
