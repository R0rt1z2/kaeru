//
// board-w7.c - LG W7 (MT6765) - kaeru board support
//
// Build 4B: build 4 with the fastboot panel actually blacked out (the K40s
// logo lives on a second surface, so video_clean_screen() alone left it up),
// the stock "oem key" registration disabled so ours stays reachable, and a
// self-describing failure path for "oem misc set".
//
// Builds 1, 2, 3, 3B, 3C and 4 remain untouched and are the fallback images.
//

#include <board_ops.h>

#include <wdt/mtk_wdt.h>

#ifndef KAERU_VERSION
#define KAERU_VERSION "3.0.0D"
#endif

#define BUILD_LABEL "build 4C"

#define VOLUME_UP   1
#define VOLUME_DOWN 17
#define POWER_KEY   8

// Stock W7 LK addresses verified against lk_a.bin.
//   video_get_rows      0x4803AA24
//   mt_disp_get_width   0x48011680
//   mt_disp_get_height  0x48011684
//   logo ready flag     0x48003AC0
static int video_get_rows(void) {
    return ((int (*)(void))(0x4803AA24 | 1))();
}

static int mt_disp_get_width(void) {
    return ((int (*)(void))(0x48011680 | 1))();
}

static int mt_disp_get_height(void) {
    return ((int (*)(void))(0x48011684 | 1))();
}

static uint32_t mt_disp_logo_context(void) {
    return ((uint32_t (*)(void))(0x48003AC0 | 1))();
}

// More stock W7 LK display entry points, all verified against lk_a.bin:
//   video_clean_screen   0x4803AA30  clears the console/text surface only
//   video_set_cursor     0x4803A9E8  (row, col) of the console surface
//   mt_disp_fill_rect    0x48005ADC  (x, y, w, h, color) on the logo surface
//   mt_disp_show_boot_logo 0x480056C4  push {r4-r11, lr} ...
static void video_clean_screen(void) {
    ((void (*)(void))(0x4803AA30 | 1))();
}

static void video_set_cursor(int row, int col) {
    ((void (*)(int, int))(0x4803A9E8 | 1))(row, col);
}

static void mt_disp_fill_rect(int x, int y, int w, int h, unsigned color) {
    ((void (*)(int, int, int, int, unsigned))(0x48005ADC | 1))(x, y, w, h, color);
}

// Build 4 called video_clean_screen() from the first stock dprintf in the
// fastboot gadget path and the K40s logo stayed up: it is drawn on a second
// surface that video_clean_screen() never touches.  Paint everything black
// first, then write one banner line on top.  patch_fastboot_startup() points
// 0x48050DA0 here and keeps mt_disp_show_boot_logo() from drawing again.
static bool fastboot_black_painted;

static void fastboot_black_screen(void) {
    int width = mt_disp_get_width();
    int height = mt_disp_get_height();

    video_clean_screen();

    if (width > 0 && height > 0)
        mt_disp_fill_rect(0, 0, width, height, 0);

    video_set_cursor(0, 0);
    video_printf("FASTBOOT (kaeru " KAERU_VERSION " - " BUILD_LABEL ")\n");

    fastboot_black_painted = true;
}

long partition_read(const char* part_name, long long offset, uint8_t* data, size_t size) {
    return ((long (*)(const char*, long long, uint8_t*, size_t))(CONFIG_PARTITION_READ_ADDRESS | 1))(
            part_name, offset, data, size);
}

// Twin of partition_read in the W7 LK (0x48053C54).  The first eight
// halfwords match read; the ninth (0xFDEF) identifies the write path.
long partition_write(const char* part_name, long long offset, uint8_t* data, size_t size) {
    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4FF0, 0xB085, 0x461F, 0x4616,
                                                     0x9003, 0x9D0F, 0xF7FE, 0xFDEF);
    if (addr)
        return ((long (*)(const char*, long long, uint8_t*, size_t))(addr | 1))(
                part_name, offset, data, size);
    return -1;
}

static bool misc_read(struct misc_message* misc_msg) {
    memset(misc_msg, 0, sizeof(*misc_msg));

    if (partition_read("misc", 0, (uint8_t*)misc_msg, sizeof(*misc_msg)) < 0) {
        printf("Failed to read misc partition\n");
        return false;
    }

    // The partition is not guaranteed to have a terminator in the command
    // field.  The bootloader_message contract uses a fixed 32-byte array.
    misc_msg->command[sizeof(misc_msg->command) - 1] = '\0';
    return true;
}

// partition_write() returns a long.  Keeping it here means "oem misc set" can
// echo the exact stock return code instead of a bare "it failed".
static long misc_write(const struct misc_message* misc_msg) {
    long ret = partition_write("misc", 0, (uint8_t*)misc_msg, sizeof(*misc_msg));

    if (ret < 0)
        printf("Failed to write misc partition (ret=%ld)\n", ret);

    return ret;
}

// Replace only the 32-byte bootloader command, preserving the rest of the
// bootloader_message.
static bool misc_write_command(const char* command) {
    struct misc_message misc_msg;

    if (!misc_read(&misc_msg))
        return false;

    memset(misc_msg.command, 0, sizeof(misc_msg.command));
    if (command && command[0])
        strncpy(misc_msg.command, command, sizeof(misc_msg.command) - 1);

    return misc_write(&misc_msg) >= 0;
}

// The stock recovery expects the BCB in misc and boot mode 2.  Setting only
// BOOTMODE_RECOVERY can boot the recovery kernel, but a recovery that checks
// the BCB may immediately return to Android.
static bool write_recovery_bcb(void) {
    if (!misc_write_command("boot-recovery"))
        return false;

    set_bootmode(BOOTMODE_RECOVERY);
    return true;
}

// The stock LK ignores boot commands written to misc.  Parse them here and
// let the dispatcher patch below handle FASTBOOT (99).
static bool parse_bootloader_messages(void) {
    struct misc_message misc_msg;

    if (!misc_read(&misc_msg))
        return false;

    bootmode_t mode = misc_command_to_bootmode(misc_msg.command);
    if (mode == BOOTMODE_NORMAL)
        return false;

    printf("Found '%s', forcing %s\n", misc_msg.command, bootmode2str(mode));

    // Sticky commands stay put so every boot lands back here, and
    // 'boot-fastboot' stays for the recovery ramdisk to read. Anything
    // else only loses the command, recovery still needs its args.
    if (!misc_command_is_sticky(misc_msg.command) &&
        !misc_command_is_for_ramdisk(misc_msg.command)) {
        memset(misc_msg.command, 0, sizeof(misc_msg.command));
        misc_write(&misc_msg);
    }

    set_bootmode(mode);
    show_bootmode(mode);

    return mode == BOOTMODE_FASTBOOT;
}

static void clear_misc_command(void) {
    misc_write_command(NULL);
}

static void cmd_reboot(const char* arg, void* data, unsigned sz) {
    const char* command = reboot_target_to_misc_command(arg);

    if (!command && arg && arg[0]) {
        fastboot_fail("unknown reboot target");
        return;
    }

    if (command) {
        struct misc_message misc_msg;

        if (!misc_read(&misc_msg)) {
            fastboot_fail("failed to read misc");
            return;
        }

        memset(misc_msg.command, 0, sizeof(misc_msg.command));
        strncpy(misc_msg.command, command, sizeof(misc_msg.command) - 1);

        if (misc_write(&misc_msg) < 0) {
            fastboot_fail("failed to write misc");
            return;
        }
    }

    fastboot_okay("");
    mtk_wdt_reset();
}

// AOSP fastboot sends RebootTo(target) as the single command
// "reboot-<target>", not as "reboot <target>".  Keep the old command names
// working by mapping each one to the same target-aware handler.
static void cmd_reboot_bootloader(const char* arg, void* data, unsigned sz) {
    (void)arg;
    cmd_reboot("bootloader", data, sz);
}

static void cmd_reboot_recovery(const char* arg, void* data, unsigned sz) {
    (void)arg;
    cmd_reboot("recovery", data, sz);
}

static void cmd_reboot_fastboot(const char* arg, void* data, unsigned sz) {
    (void)arg;
    cmd_reboot("fastboot", data, sz);
}

// oem misc [read|clear|set <command>]
static void cmd_misc(const char* arg, void* data, unsigned sz) {
    struct misc_message misc_msg;
    char op[16] = {0};
    char line[FASTBOOT_INFO_MAX + 1];
    const char* rest = arg ? next_token(arg, op, sizeof(op)) : NULL;

    if (!misc_read(&misc_msg)) {
        fastboot_fail("failed to read misc");
        return;
    }

    if (!op[0] || !strcmp(op, "read")) {
        char command[sizeof(misc_msg.command) + 1];

        memcpy(command, misc_msg.command, sizeof(misc_msg.command));
        command[sizeof(misc_msg.command)] = '\0';

        npf_snprintf(line, sizeof(line), "command: %s", command[0] ? command : "(empty)");
        fastboot_info(line);
        fastboot_okay("");
        return;
    }

    if (!strcmp(op, "clear")) {
        long ret;

        memset(misc_msg.command, 0, sizeof(misc_msg.command));

        ret = misc_write(&misc_msg);
        if (ret < 0) {
            npf_snprintf(line, sizeof(line), "misc clear: ret=%ld", ret);
            fastboot_info(line);
            fastboot_fail("failed to write misc");
            return;
        }

        fastboot_info("misc command cleared");
        fastboot_okay("");
        return;
    }

    if (!strcmp(op, "set")) {
        long ret;
        size_t len;

        while (rest && (*rest == ' ' || *rest == '\t'))
            rest++;

        if (!rest || !rest[0]) {
            fastboot_fail("usage: oem misc set <command>");
            return;
        }

        // Tolerate the spellings people actually type, so a failure here is a
        // real storage failure and not a quoting problem:
        //   oem misc set "boot-recovery"
        //   oem misc set command=boot-recovery
        len = strlen(rest);
        while (len && (rest[len - 1] == ' ' || rest[len - 1] == '\t'))
            len--;

        if (len >= 2 && rest[0] == '"') {
            rest++;
            len--;
            if (len && rest[len - 1] == '"')
                len--;
        } else if (len && rest[len - 1] == '"') {
            len--;
        }

        if (len > 8 && !strncmp(rest, "command=", 8)) {
            rest += 8;
            len -= 8;
        }

        if (!len) {
            fastboot_fail("usage: oem misc set <command>");
            return;
        }

        if (len >= sizeof(misc_msg.command)) {
            npf_snprintf(line, sizeof(line), "misc set: len=%u max=%u",
                         (unsigned)len, (unsigned)(sizeof(misc_msg.command) - 1));
            fastboot_info(line);
            fastboot_fail("command too long (max 31 chars)");
            return;
        }

        memset(misc_msg.command, 0, sizeof(misc_msg.command));

        // Byte at a time on purpose, like next_token(): rest points into the
        // fastboot command buffer wherever the "oem misc set " prefix left it
        // (cmdbuf+12, i.e. an odd address), and the optimised memcpy in
        // arch/arm/memcpy.S does a word load from that source and faults.
        // That fault never returned a status, so the host saw
        // "Status read failed" instead of an OKAY/FAIL. Read and clear only
        // touch the stack copy, which is why they never tripped it.
        for (size_t i = 0; i < len; i++) {
            misc_msg.command[i] = rest[i];
        }

        ret = misc_write(&misc_msg);
        if (ret < 0) {
            // Self-describing: the stock return code plus what we tried to
            // write, so the next report does not need a second guess.
            npf_snprintf(line, sizeof(line), "misc set: ret=%ld '%s'", ret, misc_msg.command);
            fastboot_info(line);
            fastboot_fail("failed to write misc");
            return;
        }

        npf_snprintf(line, sizeof(line), "misc: written '%s'", misc_msg.command);
        fastboot_info(line);
        fastboot_okay("");
        return;
    }

    fastboot_fail("usage: oem misc read|clear|set <command>");
}

// oem recovery - write the standard recovery BCB and reset.  This is an
// explicit, non-destructive alternative to `fastboot reboot recovery`.
static void cmd_recovery(const char* arg, void* data, unsigned sz) {
    if (!write_recovery_bcb()) {
        fastboot_fail("failed to write recovery BCB");
        return;
    }

    fastboot_okay("");
    mtk_wdt_reset();
}

// oem device-info - stable, read-only identification for scripts.
static void cmd_device_info(const char* arg, void* data, unsigned sz) {
    char line[FASTBOOT_INFO_MAX + 1];

    fastboot_info("device-info: product=LG W7");
    fastboot_info("device-info: slot-count=1");
    fastboot_info("device-info: current-slot=a");
    fastboot_info("device-info: is-userspace=no");

    npf_snprintf(line, sizeof(line), "device-info: build=kaeru %s (" BUILD_LABEL ")", KAERU_VERSION);
    fastboot_info(line);
    fastboot_okay("");
}

// Flags for oem display.  board_late_init() runs before fastboot_init(), so
// these diagnostics describe the patches that were installed for this boot
// (fastboot_black_painted, defined above, says whether they also ran).
static bool fastboot_black_patched;
static bool fastboot_stay_patched;
static bool fastboot_wdt_patched;
static bool fastboot_wdt_disabled;
static bool fastboot_logo_patched;
static bool fastboot_key_nop;

// oem display - read-only display/logo and fastboot-patch diagnostics.
static void cmd_display(const char* arg, void* data, unsigned sz) {
    char line[FASTBOOT_INFO_MAX + 1];
    int width = mt_disp_get_width();
    int height = mt_disp_get_height();
    int rows = video_get_rows();
    uint32_t clear_target = DECODE_BL_TARGET(0x48050DA0) & ~1u;

    npf_snprintf(line, sizeof(line), "display: width=%d height=%d rows=%d", width, height, rows);
    fastboot_info(line);

    npf_snprintf(line, sizeof(line), "display: logo_now=0x%08X", (unsigned)mt_disp_logo_context());
    fastboot_info(line);

    npf_snprintf(line, sizeof(line), "display: fastboot_black=%d painted=%d target=0x%08X",
                 fastboot_black_patched ? 1 : 0, fastboot_black_painted ? 1 : 0,
                 (unsigned)clear_target);
    fastboot_info(line);

    npf_snprintf(line, sizeof(line), "display: logo_off=%d key_nop=%d",
                 fastboot_logo_patched ? 1 : 0, fastboot_key_nop ? 1 : 0);
    fastboot_info(line);

    npf_snprintf(line, sizeof(line), "display: fastboot_stay=%d wdt_off=%d wdt_patch=%d",
                 fastboot_stay_patched ? 1 : 0, fastboot_wdt_disabled ? 1 : 0,
                 fastboot_wdt_patched ? 1 : 0);
    fastboot_info(line);

    fastboot_okay("");
}

// oem key - useful for checking the W7 key map.
static void cmd_key(const char* arg, void* data, unsigned sz) {
    char line[FASTBOOT_INFO_MAX + 1];

    npf_snprintf(line, sizeof(line), "VolUp=%d VolDn17=%d VolDn0=%d Power=%d",
                 mtk_detect_key(VOLUME_UP), mtk_detect_key(VOLUME_DOWN),
                 mtk_detect_key(0), mtk_detect_key(POWER_KEY));
    fastboot_info(line);
    fastboot_okay("");
}

static void cmd_bootmode(const char* arg, void* data, unsigned sz) {
    bootmode_t mode = get_bootmode();
    char line[FASTBOOT_INFO_MAX + 1];
    const char* name = bootmode2str(mode);

    npf_snprintf(line, sizeof(line), "bootmode=%d (%s)", (int)mode, name ? name : "unknown");
    fastboot_info(line);
    fastboot_okay("");
}

// Patch a BL only if the stock call site contains the expected target.  This
// keeps build 4 from silently writing into a wrong function address.
static bool patch_bl_checked(uint32_t site, uint32_t expected, uint32_t target, const char* name) {
    uint32_t current = DECODE_BL_TARGET(site) & ~1u;
    uint32_t wanted = target & ~1u;

    if (current == wanted) {
        printf("Already patched %s at 0x%08X\n", name, site);
        return true;
    }

    if (current != (expected & ~1u)) {
        printf("WARN: %s at 0x%08X targets 0x%08X, expected 0x%08X\n",
               name, site, current, expected & ~1u);
        return false;
    }

    int32_t off = (int32_t)wanted - (int32_t)(site + 4);
    if (off < -0x1000000 || off > 0xFFFFFE) {
        printf("WARN: %s at 0x%08X is out of BL range\n", name, site);
        return false;
    }

    PATCH_CALL(site, (void*)target, TARGET_THUMB);

    uint32_t applied = DECODE_BL_TARGET(site) & ~1u;
    if (applied != wanted) {
        printf("WARN: %s at 0x%08X did not take (0x%08X)\n", name, site, applied);
        return false;
    }

    printf("Patched %s at 0x%08X -> 0x%08X\n", name, site, wanted);
    return true;
}

// Fastboot-only patches.  They run only when FASTBOOT was selected, so normal
// Android boots and the existing safe commands keep their stock behaviour.
static void patch_fastboot_startup(void) {
    // The stock fastboot startup schedules a one-shot 5 s watchdog kick at
    // 0x48030ABC.  Replace that schedule with an explicit watchdog disable so
    // an idle fastboot never resets into Android.
    fastboot_wdt_patched = patch_bl_checked(0x48030ABC, 0x4801B684, 0x48007D8C,
                                            "fastboot WDT timer");

    // The first stock dprintf in the fastboot USB gadget path is at
    // 0x48050DA0.  Replace it with fastboot_black_screen(), which clears the
    // console surface, fills the whole panel black over the K40s logo and
    // writes the one fastboot banner line.
    fastboot_black_patched = patch_bl_checked(0x48050DA0, 0x4803B02C,
                                              (uint32_t)fastboot_black_screen,
                                              "fastboot screen clear");

    // Build 4 painted the panel black and the logo came back: nothing stopped
    // mt_disp_show_boot_logo() from drawing again after the clear.  Force it
    // to return as soon as we are in fastboot (stock prologue verified:
    // 0x41F0E92D = push {r4-r11, lr}).
    uint32_t logo_head = *(volatile uint32_t*)0x480056C4;
    if (logo_head == 0x41F0E92D) {
        FORCE_RETURN(0x480056C4, 0); // movs r0, #0; bx lr
        fastboot_logo_patched = true;
        printf("Disabled mt_disp_show_boot_logo at 0x480056C4\n");
    } else if (logo_head == 0x47702000) {
        fastboot_logo_patched = true;
        printf("mt_disp_show_boot_logo already disabled at 0x480056C4\n");
    } else {
        printf("WARN: unexpected mt_disp_show_boot_logo head 0x%08X\n", logo_head);
    }

    // fastboot_init() registers the stock "oem key" handler after ours, and
    // the dispatcher walks head-first, so the stock one used to win (it
    // answers "argument size is wrong").  NOP that single registration; the
    // handler itself stays untouched for normal boots.
    uint32_t key_bl = DECODE_BL_TARGET(0x48030F36) & ~1u;
    if (key_bl == 0x480302F4) {
        NOP(0x48030F36, 2);
        fastboot_key_nop = true;
        printf("NOPed stock oem key registration at 0x48030F36\n");
    } else if (*(volatile uint16_t*)0x48030F36 == 0xBF00) {
        fastboot_key_nop = true;
        printf("Stock oem key registration already NOPed at 0x48030F36\n");
    } else {
        printf("WARN: stock oem key registration at 0x48030F36 -> 0x%08X\n", key_bl);
    }

    // Safety net: if the USB gadget path ever returns, do not let mt_boot_init
    // fall through to the Android boot path.  fastboot continue still boots
    // Android from the fastboot thread on an explicit command.
    if (*(volatile uint16_t*)0x4802F580 == 0xBF30 &&
        *(volatile uint16_t*)0x4802F582 == 0xE7FE) {
        fastboot_stay_patched = true;
    } else if (*(volatile uint16_t*)0x4802F580 == 0xB011) {
        // Stock epilogue: add sp, #0x44; pop.w {r4-r11, pc}
        PATCH_MEM(0x4802F580, 0xBF30, 0xE7FE); // wfi; b .
        fastboot_stay_patched = true;
        printf("Patched fastboot return path at 0x4802F580\n");
    } else {
        printf("WARN: unexpected fastboot return path at 0x4802F580 (0x%04X)\n",
               *(volatile uint16_t*)0x4802F580);
    }
}

void board_early_init(void) {
    printf("Entering early init for LG W7 (" BUILD_LABEL ")\n");
}

void board_late_init(void) {
    printf("Entering late init for LG W7 (" BUILD_LABEL ")\n");

    uint32_t addr = 0;

    // CAUSA RAIZ do fastboot que "vira Android": o dispatcher de boot mode
    // stock (0x4802E9FC, chamado por mt_boot em 0x4802F554) NAO tem case para
    // 99 (FASTBOOT). O default em 0x4802EBEE (bne.w 0x4802EADA) segue para o
    // boot comum do Android (append "androidboot.force_normal_boot=1" e jump
    // ao kernel em 0x4802CCD0, noreturn). O fastboot_init (0x48030A20) so roda
    // se o dispatcher RETORNAR - e o unico epilogo e 0x4802EBDA (movs r0,#0;
    // add sp,#0x9c; pop.w {r4-r11,pc}), alcancado so se o boot do kernel
    // falhar.
    //
    // Patch cirurgico (byte a byte verificado com capstone) no fim da cadeia
    // mode>5 - r3 ainda e g_boot_mode (ldr r3,[r6] em 0x4802EA66, preservado
    // ate 0x4802EBEC):
    //   0x4802EBEC  cmp r3,#0xc8   ->  cmp r3,#99
    //   0x4802EBEE  bne.w default  ->  beq 0x4802EBDA (epilogo -> retorna 0)
    //   0x4802EBF0  (alta do bne.w)->  nop
    // Modo 99 -> epilogo -> mt_boot segue para fastboot_init. O modo 200 cai
    // no proprio handler (identico); 101/exoticos passam pelo prelude do 200
    // e convergem ao mesmo destino de antes; 6-9/110/0-5 intocados.
    // O padrao tem 1 unica ocorrencia no binario (verificado).
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x2B6E, 0xD072, 0x2BC8, 0xF47F, 0xAF74);
    if (addr) {
        printf("Found boot mode dispatcher at 0x%08X, adding FASTBOOT(99) case\n", addr + 4);
        PATCH_MEM(addr + 4, 0x2B63, // cmp r3, #99
                  0xD0F4,           // beq 0x4802EBDA (epilogo do dispatcher)
                  0xBF00);          // nop
    } else {
        printf("WARN: boot mode dispatcher pattern not found!\n");
    }

    // Misc first: 'adb reboot bootloader' leaves "bootloader" in misc;
    // parse_bootloader_messages() already sets mode 99.
    bool misc_fastboot = parse_bootloader_messages();

    // Register the extra OEM commands before fastboot starts.
    fastboot_register("oem misc", cmd_misc, 1);
    fastboot_register("oem key", cmd_key, 1);
    fastboot_register("oem bootmode", cmd_bootmode, 1);
    fastboot_register("oem recovery", cmd_recovery, 1);
    fastboot_register("oem device-info", cmd_device_info, 1);
    fastboot_register("oem display", cmd_display, 1);

    // Build 4: Vol-Up is not a menu trigger anymore.  It directly selects
    // fastboot, exactly like build 2 did, and removes any stale misc command
    // that could redirect this boot.
    if (mtk_detect_key(VOLUME_UP)) {
        clear_misc_command();
        set_bootmode(BOOTMODE_FASTBOOT);
        show_bootmode(BOOTMODE_FASTBOOT);
        printf("Vol-Up: forcing FASTBOOT\n");
    }

    bool fastboot_selected = misc_fastboot || get_bootmode() == BOOTMODE_FASTBOOT;

    if (fastboot_selected) {
        // Belt and suspenders: disable the WDT before fastboot_init, then the
        // fastboot path also replaces its one-shot WDT timer with a disable.
        mtk_wdt_disable();
        fastboot_wdt_disabled = true;

        patch_fastboot_startup();
    }

    // Familia de 'reboot' do W7: quatro bl -> fastboot_register encadeados
    // (reboot / reboot-bootloader / reboot-recovery / reboot-fastboot) em
    // 0x48030D68..0x48030DA4 (0x14 de passo). NOPar os quatro e registrar os
    // nossos; 'continue' (0x48030D54) fica intacto.  O host manda
    // "reboot-<target>" em RebootTo(), por isso os tres aliases sao
    // registrados separadamente.
    NOP(0x48030D68, 2); // reboot
    NOP(0x48030D7C, 2); // reboot-bootloader
    NOP(0x48030D90, 2); // reboot-recovery
    NOP(0x48030DA4, 2); // reboot-fastboot
    fastboot_register("reboot", cmd_reboot, 1);
    fastboot_register("reboot-bootloader", cmd_reboot_bootloader, 1);
    fastboot_register("reboot-recovery", cmd_reboot_recovery, 1);
    fastboot_register("reboot-fastboot", cmd_reboot_fastboot, 1);

    // Verified boot warning: 2B02 D052 2B03 D03A bate em 0x48027852 (igual ao
    // mcaymanlm). 2 = orange; NOPar o beq faz cair fora do aviso.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x2B02, 0xD052, 0x2B03, 0xD03A);
    if (addr) {
        printf("Found orange state warning at 0x%08X\n", addr + 2);
        NOP(addr + 2, 1);
    }
}
