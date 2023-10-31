#include "include/libc.h"
#include "include/lk_api.h"

uint64_t g_boot, g_lk, g_para, g_recovery;

void hexdump(const void* data, size_t size) {
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

void cmd_hexdump(const char *arg, void *data, unsigned sz) {
    char *args = strdup(arg);
    char *a = strtok(args, " ");
    char *s = strtok(NULL, " ");

    if (a != NULL && s != NULL) {
        long al = strtol(a, NULL, 0);
        long sl = strtol(s, NULL, 0);
        hexdump((void *)al, sl);
    } else {
        video_printf("Invalid arguments\n");
    }

    fastboot_okay("");
}

void patch_lk() {
    // Disable orange state warning
    volatile uint16_t *x = (volatile uint16_t *)0x41e03a48;
    x[0] = 0x2000;
    x[1] = 0x4770;

    // Disable red state warning
    x = (volatile uint16_t *)0x41e039ec;
    x[0] = 0x2000;
    x[1] = 0x4770;

    // Force enable FRP unlock
    x = (volatile uint16_t *)0x41e1f86c;
    x[0] = 0x2001;
    x[1] = 0x6008;
    x[2] = 0x2000;
    x[3] = 0x4770;

    // Force green state
    ((volatile uint32_t *)0x41e44518)[0] = BOOTSTATE_GREEN;

    // Force enable UART
    *((volatile uint8_t *)(0x41e370cc + 20)) = '0';
    *((volatile uint8_t *)(0x41e367a4 + 21)) = '0';

    // Disable low battery check
    x = (volatile uint16_t *)0x41e1ed84;
    x[0] = 0x2001;
    x[1] = 0x4770;
}

void parse_gpt() {
    uint8_t raw[0x1000] = {0};
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

__attribute__((section(".text.main"))) int main(void) {
    patch_lk();
    parse_gpt();

    video_printf("> Entered kaeru payload - (%d)\n",
                 *g_boot_mode);

    fastboot_register("oem hexdump", cmd_hexdump, 1);

    app();

    video_printf("Oops, original app() returned!\n");
    while (1) {
        // WDT should kick in after 30 seconds ~
    }
}
