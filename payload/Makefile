CC := arm-none-eabi-gcc
AS := arm-none-eabi-as
LD := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy

CFLAGS := -std=gnu99 -Os -mthumb -mcpu=cortex-a9 -fno-builtin-printf -fno-strict-aliasing -fno-builtin-memcpy -mno-unaligned-access -DPRINTF_DISABLE_SUPPORT_FLOAT=1 -fPIE
LDFLAGS := -T linker.x -nodefaultlibs -nostdlib

BUILD_DIR := ./build

TARGET := payload

C_SRC = main.c libc.c

OBJ = $(C_SRC:%.c=$(BUILD_DIR)/%.o)
DEP = $(OBJ:%.o=%.d)

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $^ $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJ)
	$(LD) -o $@ $^ $(LDFLAGS)

-include $(DEP)

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(@D)
	$(CC) -MMD -c -o $@ $< $(CFLAGS)

clean:
	-rm -rf $(BUILD_DIR)
