# Standalone build driving the STM32CubeIDE toolchain, mirrors the IDE "Debug" configuration.
# Override CUBEIDE_PLUGINS if CubeIDE is installed elsewhere. The versioned plugin
# directories are globbed so a CubeIDE update does not break this file.

TARGET    := windboss-l051-turbine
BUILD_DIR := build
DEBUG     := 1

CUBEIDE_PLUGINS ?= C:/ST/STM32CubeIDE_2.2.0/STM32CubeIDE/plugins

GCC_PATH  := $(dir $(firstword $(wildcard $(CUBEIDE_PLUGINS)/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*/tools/bin/arm-none-eabi-gcc.exe)))
OPENOCD   := $(firstword $(wildcard $(CUBEIDE_PLUGINS)/com.st.stm32cube.ide.mcu.externaltools.openocd.win32_*/tools/bin/openocd.exe))
OCD_SCRIPTS := $(firstword $(wildcard $(CUBEIDE_PLUGINS)/com.st.stm32cube.ide.mcu.debug.openocd_*/resources/openocd/st_scripts))

PREFIX := $(GCC_PATH)arm-none-eabi-
CC     := $(PREFIX)gcc
AS     := $(PREFIX)gcc -x assembler-with-cpp
OBJCOPY:= $(PREFIX)objcopy
SIZE   := $(PREFIX)size

C_SOURCES := $(wildcard Core/Src/*.c) $(wildcard Drivers/STM32L0xx_HAL_Driver/Src/*.c)
ASM_SOURCES := Core/Startup/startup_stm32l051k8tx.s

MCU := -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft

C_DEFS := -DUSE_HAL_DRIVER -DSTM32L051xx
C_INCLUDES := \
  -ICore/Inc \
  -IDrivers/STM32L0xx_HAL_Driver/Inc \
  -IDrivers/STM32L0xx_HAL_Driver/Inc/Legacy \
  -IDrivers/CMSIS/Device/ST/STM32L0xx/Include \
  -IDrivers/CMSIS/Include

ifeq ($(DEBUG),1)
OPT    := -Og
C_DEFS += -DDEBUG
DBG    := -g3 -gdwarf-2
else
OPT := -Os
DBG :=
endif

# recursive assignment, $@ must expand per-target for the dependency file
CFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) $(DBG) \
  -Wall -Wno-discarded-qualifiers -ffunction-sections -fdata-sections \
  -MMD -MP -MF"$(@:%.o=%.d)"

ASFLAGS := $(MCU) $(OPT) $(DBG) -Wall -ffunction-sections -fdata-sections

LDSCRIPT := STM32L051K8TX_FLASH.ld
LDFLAGS  := $(MCU) -specs=nano.specs -T$(LDSCRIPT) -lc -lm -lnosys \
  -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections

OBJECTS := $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(ASFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf
	$(OBJCOPY) -O binary -S $< $@

$(BUILD_DIR):
	mkdir $(BUILD_DIR)

OCD := "$(OPENOCD)" -s "$(OCD_SCRIPTS)" -s "$(CURDIR)" -f $(TARGET).cfg

# The bare `halt` is what makes this reliable: `program` starts with `reset init`, and
# NRST clears DBGMCU_CR, so a firmware that re-enters WFI cannot be caught at the vector.
# `reset exit` is deliberately not used: it does `poll off` before its `reset run`, so the
# core is never resumed before `shutdown` tears the session down.
OCD_PROGRAM := -c "init" -c "halt" \
	  -c "program $(BUILD_DIR)/$(TARGET).elf verify" \
	  -c "reset run" \
	  -c "shutdown"

flash: $(BUILD_DIR)/$(TARGET).elf
	$(OCD) $(OCD_PROGRAM)

# flashes whatever is already in build/, without rebuilding first
flash-only:
	$(OCD) $(OCD_PROGRAM)

reset:
	$(OCD) -c "init" -c "reset run" -c "shutdown"

clean:
	rm -rf $(BUILD_DIR)

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all flash flash-only reset clean
