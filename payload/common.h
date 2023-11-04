#include <stdint.h>

#define LK_BASE 0x41e00000
#define PAYLOAD_DST 0x41E5FE50 // End of (LK) stack is 0x41e5fdd4.
#define PAYLOAD_CALLER 0x41e19284 // blx app() - replaced with bl payload().

uint32_t* g_boot_mode = (uint32_t*)0x41e4c830;

struct device_t {
    uint32_t unk1;
    uint32_t unk2;
    uint32_t unk3;
    uint32_t unk4;
    size_t (*read)(struct device_t *dev, uint64_t dev_addr, void *dst, uint32_t size, uint32_t part);
    size_t (*write)(struct device_t *dev, void *src, uint64_t block_off, uint32_t size, uint32_t part);
};

int (*app)() = (void*)(0x41e1a6bc|1);
struct device_t* (*get_device)() = (void*)(0x41e04ad8|1);
size_t (*video_printf)(const char *format, ...) = (void *)(0x41e1e7cc|1);

typedef enum {
    BOOTMODE_NORMAL = 0,
    BOOTMODE_META = 1,
    BOOTMODE_RECOVERY = 2,
    BOOTMODE_FACTORY = 4,
    BOOTMODE_ADVMETA = 5,
    BOOTMODE_ATEFACT = 6,
    BOOTMODE_ALARM = 7,
    BOOTMODE_FASTBOOT = 99,
} bootmode_t;

void (*fastboot_info)(const char *reason) = (void*)(0x41e1ab20|1);
void (*fastboot_fail)(const char *reason) = (void*)(0x41e1ab68|1);
void (*fastboot_okay)(const char *reason) = (void*)(0x41e1acf0|1);
void (*fastboot_register)(const char *prefix, void (*handle)(const char *arg, void *data, unsigned sz), unsigned char security_enabled) = (void*)(0x41e1a920|1);

#define USER_PART 8