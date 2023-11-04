#include <stddef.h>

#include "common.h"
#include "libc.h"

uint64_t g_boot, g_lk, g_para, g_recovery;

// See https://github.com/amonet-kamakiri/kamakiri/blob/master/brom-payload/payload_common.c#L22
void hex_dump(const void* data, size_t size) {
    size_t i, j;
    for (i = 0; i < size; ++i) {
        video_printf("%02X ", ((unsigned char*)data)[i]);
        if ((i+1) % 8 == 0 || i+1 == size) {
            video_printf(" ");
            if ((i+1) % 16 == 0) {
                video_printf("\n");
            } else if (i+1 == size) {
                if ((i+1) % 16 <= 8) {
                    video_printf(" ");
                }
                for (j = (i+1) % 16; j < 16; ++j) {
                    video_printf("   ");
                }
                video_printf("\n");
            }
        }
    }
}

static void parse_gpt() {
    uint8_t raw[0x1000] = { 0 };
    struct device_t *dev = get_device();
    dev->read(dev, 0x400, raw, sizeof(raw), USER_PART);

    for (int i = 0; i < sizeof(raw) / 0x80; ++i) {
        uint8_t *ptr = &raw[i * 0x80];
        uint8_t *name = ptr + 0x38;
        uint32_t start;
        memcpy(&start, ptr + 0x20, 4);

        if (utf16le_strcmp(name, "boot") == 0) {
            g_boot = start;
        } else if (utf16le_strcmp(name, "lk") == 0) {
            g_lk = start;
        } else if (utf16le_strcmp(name, "para") == 0) {
            g_para = start;
        } else if (utf16le_strcmp(name, "recovery") == 0) {
            g_recovery = start;
        }
    }
}

void enable_uart() {
    // Configure both UART strings to always force enable UART.
    char* printk_disable_uart1 = (char *)0x41e3b354;
    char* printk_disable_uart2 = (char *)0x41e3bcd0;
    strcpy(printk_disable_uart1, " printk.disable_uart=0");
    strcpy(printk_disable_uart2, "printk.disable_uart=0");
}

void patch_functions() {
    // sec_usbdl_enabled()
    uint16_t *patch = (void *)0x41e260a8;
    *patch++ = 0x2000; // movs r0, #0
    *patch = 0x4770; // bx lr
}

bootmode_t read_boot_mode(struct device_t *dev) {
    uint8_t bootloader_msg[0x20] = { 0 };

    if (!g_para || !dev->read(dev, g_para * 0x200, bootloader_msg,
                              sizeof(bootloader_msg), USER_PART)) {
        video_printf("Unable to read bootloader message!\n");
        return BOOTMODE_NORMAL;
    }

    bootloader_msg[sizeof(bootloader_msg) - 1] = '\0';
    if (strcmp((char *)bootloader_msg, "boot-recovery") == 0) {
        memset(bootloader_msg, 0, sizeof(bootloader_msg));
        return BOOTMODE_RECOVERY;
    } else if (strcmp((char *)bootloader_msg, "boot-fastboot") == 0) {
        memset(bootloader_msg, 0, sizeof(bootloader_msg));
        return BOOTMODE_FASTBOOT;
    }

    return BOOTMODE_NORMAL;
}

__attribute__ ((section(".text.main")))
int main(void) {
    // Perform basic early initialization.
    parse_gpt();
    struct device_t *dev = get_device();

    // Function / memory patching.
    enable_uart();
    patch_functions();

    // Give priority to the boot mode stored in the bootloader
    // message.
    uint32_t m_boot_mode = read_boot_mode(dev);
    if (m_boot_mode != BOOTMODE_NORMAL) {
        *g_boot_mode = m_boot_mode;
    }

    // Use META mode to force fastboot mode. This is done because
    // volume down key triggers META mode so there's no way to get
    // into fastboot mode otherwise.
    if (*g_boot_mode == BOOTMODE_META) {
        *g_boot_mode = BOOTMODE_FASTBOOT;
    }

    // Once we're done, jump back to the real LK.
    app();

    // If we get here, something went really wrong.
    video_printf("Oops, original app() returned!\n");
    while (1) {
        // Do nothing until watchdog resets us.
    }
}