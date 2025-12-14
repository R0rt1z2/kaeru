//
// SPDX-FileCopyrightText: 2025 R0rt1z2 <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define GPIO_DIN3 0x10005520

#define BACKUP_SRC 0x200000
#define MICROLOADER_BACKUP 0x44100000
#define MICROLOADER_SIZE 0x400
#define MICROLOADER_SRC (0x4BD5C000 - 0x50)
#define PAYLOAD_DST 0x41000000
#define PAYLOAD_SIZE 0x80000
#define PAYLOAD_SRC 0x80000

uint32_t pmic_read_interface(uint32_t reg, uint32_t* val, uint32_t mask, uint32_t shift) {
    return ((uint32_t (*)(uint32_t, uint32_t*, uint32_t, uint32_t))(0x4BD15664 | 1))(reg, val, mask,
                                                                                     shift);
}

int is_privacy_pressed(void) {
    uint32_t reg = 0;

    pmic_read_interface(0x142, &reg, 1, 1);

    return !reg;
}

int is_volume_up_pressed(void) {
    uint32_t reg_val = *(volatile uint32_t *)GPIO_DIN3;
    return ((reg_val >> 4) & 1) == 0; // GPIO_ACTIVE_LOW, pin 36, bit 4
}

int is_volume_down_pressed(void) {
    uint32_t reg_val = *(volatile uint32_t *)GPIO_DIN3;
    return ((reg_val >> 5) & 1) == 0; // GPIO_ACTIVE_LOW, pin 37, bit 5
}

struct device_t* mt_part_get_device(void) {
    return ((struct device_t* (*)(void))(CONFIG_MT_PART_GET_DEVICE_ADDRESS|1))();
}

part_t* mt_get_part(const char* name) {
    return ((part_t* (*)(const char*))(CONFIG_MT_PART_GET_PARTITION_ADDRESS|1))(name);
}

int is_key_pressed(int key) {
    return ((int (*)(int))(0x4BD11190|1))(key);
}

int recovery_keys(void) {
    return ((int (*)(void))(0x4BD0DA88|1))();
}

void cmd_flash(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(0x4BD2AE24|1))(arg, data, sz);
}

void cmd_reboot(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(0x4BD29370|1))(arg, data, sz);
}

uint32_t pmic_config_interface(uint32_t reg, uint32_t val, uint32_t mask, uint32_t shift) {
    return ((uint32_t (*)(uint32_t, uint32_t, uint32_t, uint32_t))(0x4BD156A4 | 1))(reg, val, mask, shift);
}

size_t (*original_read)(struct device_t *dev, uint64_t block_off, void *dst, uint32_t sz, uint32_t part);

void cmd_help(const char *arg, void *data, unsigned sz) {
    struct fastboot_cmd *cmd = (struct fastboot_cmd *)0x4BD731EC;

    if (!cmd) {
        fastboot_fail("No commands found!");
        return;
    }

    fastboot_info("Available oem commands:");
    while (cmd) {
        if (cmd->prefix) {
            if (strncmp(cmd->prefix, "oem", 3) == 0) {
                fastboot_info(cmd->prefix);
            }
        }
        cmd = cmd->next;
    }

    fastboot_okay("");
}

uint64_t get_partition_sector(const char *partition_name) {
    part_t* part = mt_get_part(partition_name);
    if (!part) {
        return 0;
    }
    return (part->start_sect);
}

static void write_boot_command(const char *command) {
    struct misc_message misc_msg = {0};
    strncpy(misc_msg.command, command, 31);

    struct device_t* dev = mt_part_get_device();
    if (!dev || dev->init != 1) {
        fastboot_fail("Block device not initialized!");
        return;
    }

    uint64_t part_offset = get_partition_sector("MISC") * BLOCK_SIZE;
    if (dev->write(dev, &misc_msg, part_offset, sizeof(misc_msg), USER_PART) < 0) {
        fastboot_fail("Failed to write bootloader message!");
        return;
    }
}

static int inject_microloader(void *data, unsigned sz, const char *partition_name)
{
    uint8_t *image_data = (uint8_t *)data;
    
    fastboot_info("");
    fastboot_info("[amonet] Injecting microloader...");

    if (sz < 0x800) {
        fastboot_info("[amonet] Image too small to inject microloader");
        return -1;
    }
    
    if (memcmp(image_data + 0x400, BOOTIMG_MAGIC, BOOTIMG_MAGIC_SZ) == 0) {
        fastboot_info("[amonet] Microloader already injected");
        return 0;
    }
    
    if (memcmp(image_data, BOOTIMG_MAGIC, BOOTIMG_MAGIC_SZ) != 0) {
        fastboot_info("[amonet] Not a valid Android boot image");
        return -1;
    }
    
    uint8_t *microloader = (uint8_t *)MICROLOADER_BACKUP;
    memcpy(image_data + 0x400, image_data, 0x400);
    memcpy(image_data, microloader, MICROLOADER_SIZE);
    
    fastboot_info("[amonet] OK");
    return 0;
}

static int flash_payload(void *data, unsigned sz)
{
    struct device_t* dev = mt_part_get_device();

    fastboot_info("");
    fastboot_info("[amonet] Flashing LK payload...");

    if (sz > PAYLOAD_SIZE) {
        fastboot_fail("[amonet] Payload too large");
        return -1;
    }

    size_t ret = dev->write(dev, data, PAYLOAD_SRC, sz, BOOT0_PART);
    if (ret != sz) {
        fastboot_info("[amonet] Failed to write main payload");
        return -1;
    }

    fastboot_info("[amonet] OK");

    fastboot_info("[amonet] Flashing backup payload...");
    ret = dev->write(dev, data, BACKUP_SRC, sz, BOOT0_PART);
    if (ret != sz) {
        fastboot_fail("[amonet] Failed to write backup payload");
        return -1;
    }

    fastboot_info("[amonet] OK");
    return 0;
}

void cmd_flash_wrapper(const char *arg, void *data, unsigned sz)
{
    if (strcmp(arg, "boot") == 0 || strcmp(arg, "recovery") == 0) {
        if (inject_microloader(data, sz, arg) < 0) {
            fastboot_fail("Failed to inject microloader");
            return;
        }
    }
    else if (strcmp(arg, "lkp") == 0) {
        if (flash_payload(data, sz) < 0) {
            fastboot_fail("Failed to flash LK payload");
        } else {
            fastboot_okay("");
        }
        return;
    }
    else if (strstr(arg, "_amonet")) {
        char new_arg[32] = {0};
        strncpy(new_arg, arg, strstr(arg, "_amonet") - arg);
        cmd_flash(new_arg, data, sz);
        return;
    }

    cmd_flash(arg, data, sz);
}

void cmd_reboot_wrapper(const char *arg, void *data, unsigned sz)
{
    if (arg && strstr(arg, "recovery")) {
        write_boot_command("boot-recovery");
    }

    if (arg && strstr(arg, "bootloader")) {
        write_boot_command("boot-amonet");
    }

    cmd_reboot("", data, sz);
}

int sixty_four_kernel(uint8_t *bootopt)
{
    int i = 0;

    for (; i < (BOOTIMG_ARGS_SZ-0x16); i++) {
        if (0 == strncmp((char*)&bootopt[i], "bootopt=", sizeof("bootopt=")-1)) {
            if (0 == strncmp((char*)&bootopt[i+0x12], "64", sizeof("64")-1)) {
                return 1;
            }
            if (0 == strncmp((char*)&bootopt[i+0x12], "32", sizeof("32")-1)) {
                return 0;
            }
        }
    }

    return 0;
}

int read_func(struct device_t *dev, uint64_t block_off, void *dst, size_t sz, int part)
{
    printf("read_func hook\n");

    uint64_t g_boot = get_partition_sector("boot");
    uint64_t g_recovery = get_partition_sector("recovery");

    int ret = 0;
    if (block_off == g_boot * 0x200 || block_off == g_recovery * 0x200) {
        printf("demangle boot image - from 0x%08X\n", __builtin_return_address(0));

        if (sz < 0x400) {
            ret = original_read(dev, block_off + 0x400, dst, sz, part);
        }
        else {
            void *second_copy = (char *)dst + 0x400;
            ret = original_read(dev, block_off, dst, sz, part);
            memcpy(dst, second_copy, 0x400);
            memset(second_copy, 0, 0x400);
        }
    }
    else {
        printf("normal read - from 0x%08X\n", __builtin_return_address(0));
        ret = original_read(dev, block_off, dst, sz, part);
    }
    return ret;
}

void parse_bootloader_messages(void) {
    struct device_t* dev = mt_part_get_device();
    if (!dev || dev->init != 1) {
        printf("Block device not initialized for MISC reading\n");
        return;
    }

    part_t* misc_part = mt_get_part("MISC");
    if (!misc_part) {
        printf("MISC partition not found\n");
        return;
    }

    uint8_t bootloader_msg[0x20] = {0};
    uint64_t misc_offset = get_partition_sector("MISC") * BLOCK_SIZE;

    dev->read(dev, misc_offset, bootloader_msg, 0x20, USER_PART);
    printf("Read bootloader_msg: %s\n", bootloader_msg);

    if (strncmp((char*)bootloader_msg, "boot-amonet", 11) == 0) {
        printf("Found boot-amonet command, forcing fastboot\n");
        set_bootmode(BOOTMODE_FASTBOOT);
        memset(bootloader_msg, 0, 0x10);
        dev->write(dev, bootloader_msg, misc_offset, 0x10, USER_PART);
    }
    else if (strncmp((char*)bootloader_msg, "FASTBOOT_PLEASE", 15) == 0) {
        if (get_bootmode() == BOOTMODE_RECOVERY) {
            memset(bootloader_msg, 0, 0x10);
            dev->write(dev, bootloader_msg, misc_offset, 0x10, USER_PART);
        }
        else {
            printf("Found FASTBOOT_PLEASE command, forcing fastboot\n");
            set_bootmode(BOOTMODE_FASTBOOT);
        }
    }
    else if (strncmp((char*)bootloader_msg, "boot-recovery", 13) == 0) {
        printf("Found boot-recovery command, forcing recovery\n");
        set_bootmode(BOOTMODE_RECOVERY);
        memset(bootloader_msg, 0, 0x10);
        dev->write(dev, bootloader_msg, misc_offset, 0x10, USER_PART);
    }

    if (strncmp((char*)bootloader_msg + 0x10, "UART_PLEASE", 11) == 0) {
        printf("Found UART_PLEASE, enabling UART\n");
        strcpy((char *)0x4BD4D313, " printk.disable_uart=0");
    }
}

void board_early_init(void) {
    printf("Entering early init for Echo Show 5 2nd Gen (2021)\n");

    // Amazon uses IDME to check whether the device is locked or unlocked.
    // These functions verify if the unlock code matches the one stored in
    // the IDME partition and perform additional unlock verification checks.
    //
    // We patch them to bypass security checks and allow unrestricted fastboot
    // access regardless of the actual device lock state.
    FORCE_RETURN(0x4BD01EF4, 1);

    // This function is called by the unlock verification above to validate the
    // actual unlock code against the IDME data. However, it also gets called
    // from other parts of the bootloader, so we need to patch it as well to avoid
    // any conflicts or security check failures.
    //
    // When verification fails, it returns -1, so we patch it to always return
    // 0 (success) to ensure consistent unlock behavior throughout the system.
    FORCE_RETURN(0x4BD02068, 0);

    // The following patches disable specific video_printf calls that display
    // mode-specific messages during boot. These default messages can be
    // confusing to end users and don't provide useful information.
    //
    // We disable them here and will display our own custom messages in
    // board_late_init instead..
    NOP(0x4BD120CA, 2); // => FASTBOOT mode...
    NOP(0x4BD283DA, 2); // => FASTBOOT mode...
    NOP(0x4BD283B8, 2); // => FACTORYRESET mode...
}

void board_late_init(void) {
    printf("Entering late init for Echo Show 5 2nd Gen (2021)\n");

    // Retrieve the Preloader boot arguments
    uint32_t **argptr = (void *)0x4BD00020;
    uint32_t *o_boot_mode = (uint32_t *)*argptr + 1; // argptr boot mode

    // Parse any existing bootloader messages
    parse_bootloader_messages();
    
    // If the privacy button is held during boot, force the device into
    // FASTBOOT mode to allow easy access for debugging or flashing.
    if (is_privacy_pressed()) {
        set_bootmode(BOOTMODE_FASTBOOT);
    }

    // Use factory mode to force fastboot
    if (*o_boot_mode == BOOTMODE_FACTORY) {
        set_bootmode(BOOTMODE_FASTBOOT);
    }

    // Use advanced factory mode to force recovery
    else if (*o_boot_mode == BOOTMODE_ATEFACT) {
        set_bootmode(BOOTMODE_RECOVERY);
    }

    // Retrieve the block device
    struct device_t* dev = mt_part_get_device();
    if (!dev || dev->init != 1) {
        printf("Block device not initialized!\n");
        return;
    }

    if (get_bootmode() == BOOTMODE_FASTBOOT) {
        // Disable built-in command(s)
        NOP(0x4BD28CB2, 2); // fastboot flash
        NOP(0x4BD28CEA, 2); // fastboot reboot
        NOP(0x4BD28CFC, 2); // fastboot reboot-bootloader

        // Register our custom command(s)
        fastboot_register("reboot", cmd_reboot_wrapper, 1);
        fastboot_register("flash:", cmd_flash_wrapper, 1);
        fastboot_register("oem help", cmd_help, 1);

        // This is so people can't re-run fastbrick
        fastboot_publish("is-amonet", "1");
    } else {
        // Hook bootimg read function
        original_read = (void *)dev->read;
        uint32_t *patch32 = (void *)&dev->read;
        *patch32 = (uint32_t)read_func;

        // Allow booting 64-bit kernels if specified
        uint8_t dst[0x800] = {0};
        bootmode_t mode = get_bootmode();
        dev->read(
            dev, (mode == BOOTMODE_RECOVERY ? 
            get_partition_sector("recovery") : 
            get_partition_sector("boot")) * 0x200,
            dst, sizeof(dst), USER_PART
        );
        boot_img_hdr *hdr = (boot_img_hdr *)dst;

        if (memcmp(hdr->magic, BOOTIMG_MAGIC, BOOTIMG_MAGIC_SZ) == 0) {
            if (sixty_four_kernel(hdr->cmdline)) {
                printf("64-bit kernel detected, forcing 64-bit mode\n");
                (*argptr)[0x53] = 4;
                WRITE32(0x4BD701B8, 1);
            }
        }
    }

    if (get_bootmode() != BOOTMODE_NORMAL && get_bootmode() != BOOTMODE_FASTBOOT) {
        // Show the current boot mode on screen when not performing a normal boot.
        // This is standard behavior in many LK images, but not in this one by default.
        //
        // Displaying the boot mode can be helpful for developers, as it provides
        // immediate feedback and can prevent debugging headaches.
        show_bootmode(get_bootmode());
    }

    if (get_bootmode() == BOOTMODE_FASTBOOT) {
        video_printf(" => HACKED FASTBOOT mode: (%d) - xyz, k4y0z, R0rt1z2\n", *o_boot_mode);
    }

    // Enable back long power key press to force shutdown
    pmic_config_interface(0x011A, 0x1, 0x1, 6);
}
