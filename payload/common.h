#include <stdint.h>

#define LK_BASE 0x41e00000
#define PAYLOAD_DST 0x41e58648 // End of (LK) stack is 0x41e585cc.
#define PAYLOAD_CALLER 0x41e1b9b4 // blx app() - replaced with bl payload().

uint32_t* g_boot_mode = (uint32_t*)0x41e44500;

struct device_t {
    uint32_t unk1;
    uint32_t unk2;
    uint32_t unk3;
    uint32_t unk4;
    size_t (*read)(struct device_t *dev, uint64_t dev_addr, void *dst, uint32_t size, uint32_t part);
    size_t (*write)(struct device_t *dev, void *src, uint64_t block_off, uint32_t size, uint32_t part);
};

typedef enum {
    BOOTMODE_NORMAL = 0,
    BOOTMODE_RECOVERY = 2,
    BOOTMODE_FASTBOOT = 99,
} bootmode_t;

int (*app)() = (void*)(0x41e1ccc8|1);
struct device_t* (*get_device)() = (void*)(0x41e14050|1);
size_t (*video_printf)(const char *format, ...) = (void *)(0x41e20c0c|1);

void (*fastboot_info)(const char *reason) = (void*)(0x41e1d18c|1);
void (*fastboot_fail)(const char *reason) = (void*)(0x41e1d1d4|1);
void (*fastboot_okay)(const char *reason) = (void*)(0x41e1d37c|1);
void (*fastboot_register)(const char *prefix, void (*handle)(const char *arg, void *data, unsigned sz), unsigned char security_enabled) = (void*)(0x41e1cf8c|1);

#define USER_PART 8