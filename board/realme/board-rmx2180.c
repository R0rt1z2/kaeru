//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <me@r0rt1z2.com>
// SPDX-FileCopyrightText: 2026 fantom3031 <mpiven69@gmail.com>
// SPDX-FileCopyrightText: 2025 antagonizzzt <antagonizzzt@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//


#include <board_ops.h>
#include <lib/thread.h>
#include <wdt/mtk_wdt.h>
#include <stdlib.h> 
#include <string.h>
#include <ctype.h>

#define VOLUME_UP 17
#define VOLUME_DOWN 1
#define POWER_KEY 8

#define BG_COLOUR FB_TEAL
#define TITLE_COLOUR FB_LIME
#define OPTIONS_COLOUR FB_WHITE
#define ROUND_RECT_COLOUR FB_ORANGE
#define ROUND_RECT_RADIUS 8

#define CLEAN_SCREEN            0x48032038
#define GET_ROWS                0x4803202c
#define GET_COLUMNS             0x482862b8   
#define SET_CURSOR              0x48031ff0

#define SHOW_LOGO_ADDR          0x4801ad28
#define VENDOR_SHOW_LOGO        0x4801ae8c
#define MDELAY_ADDR             0x48004cac
#define CONTINUE_ADDR           0x48027578

#define POWER_OFF_ADDR          0x48006b34
#define DRINTF_ADDR             0x480322b8
#define REBOOT_RECOVERY_ADDR    0x480277b4
#define REBOOT_BL_ADDR          0x48027bc4
#define REBOOT_ADDR             0x48027b94
#define DISP_UPDATE_ADDR        0x480010fc
//for spoof
#define FASTBOOT_HNDLR1         0x480265bc
#define FASTBOOT_HNDLR2         0x480265ea
#define ENV_INIT_CALL           0x48002d7e
#define ENV_INIT_ADDR           0x4804a8f0
#define DATA_WIPE_ADDR          0x4802dd64

void fb_text(uint32_t x, uint32_t y, const char *str, uint32_t color);

static void video_clean_screen(void) {
    ((void (*)(void))(CLEAN_SCREEN | 1))();
}

static int video_get_rows(void) {
    return ((int (*)(void))(GET_ROWS | 1))();
}

static int video_get_columns(void) {
    return READ32(GET_COLUMNS);
}

static void video_set_cursor(int row, int col) {
    ((void (*)(int, int))(SET_CURSOR | 1))(row, col);
}

static int dprintf(const char* fmt, ...) {
    return ((int (*)(const char*))(DRINTF_ADDR | 1))(fmt);
}

static void mt_disp_show_boot_logo(void) {
    ((void (*)(void))(SHOW_LOGO_ADDR | 1))();
}

static void mdelay(unsigned long msecs) {
    ((void (*)(unsigned long))(MDELAY_ADDR | 1))(msecs);
}

static void cmd_continue(void) {
    ((void (*)(void))(CONTINUE_ADDR | 1))();
}

static void cmd_reboot_recovery(void) {
    ((void (*)(void))(REBOOT_RECOVERY_ADDR | 1))();
}

static void mt_power_off(void) {
    ((void (*)(void))(POWER_OFF_ADDR | 1))();
}

static void cmd_reboot_bootloader(void) {
    ((void (*)(void))(REBOOT_BL_ADDR | 1))();
}

static void cmd_reboot(void) {
    ((void (*)(void))(REBOOT_ADDR | 1))();
}

void vendor_show_logo(int param_1) {
    ((void (*)(void))(VENDOR_SHOW_LOGO | 1))();
}

static void mt_disp_update(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    ((void (*)(uint32_t, uint32_t, uint32_t, uint32_t))(DISP_UPDATE_ADDR | 1))(x, y, width, height);
}

static void show_main_menu(void);
static void show_spoof_menu(void);

static void env_init(void) {    
((void (*)(void))(ENV_INIT_ADDR | 1))();
}

static int set_env(char *name, char *value) {
 uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0x2200, 0xF7FF, 0xBED7, 0xBF00);
    if (addr) {
        printf("Found set_env at 0x%08X\n", addr);
        return ((int (*)(char *, char *))(addr | 1))(name, value);
    }
    return -1;
}

static char *get_env(char *name) {
 uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0xB510, 0x4602, 0x4604, 0x4909);
    if (addr) {
        printf("Found get_env at 0x%08X\n", addr);
        return ((char* (*)(char *name))(addr | 1))(name);
    }
    return NULL;
}

static void data_wipe(void) {
    ((void (*)(void))(DATA_WIPE_ADDR | 1))();
}

long partition_read(const char* part_name, long long offset, uint8_t* data, size_t size) {
    return ((long (*)(const char*, long long, uint8_t*, size_t))(CONFIG_PARTITION_READ_ADDRESS | 1))(
            part_name, offset, data, size);
}

ssize_t read_partition(const char* part_name, void* buf, size_t size) {
    if (!part_name || !buf || size == 0)
        return -1;
    ssize_t result = partition_read(part_name, 0, (uint8_t*)buf, size);
    if (result < 0) {
        printf("Failed to read partition '%s'\n", part_name);
        return -1;
    }
    printf("Read %zd bytes from '%s'\n", result, part_name);
    return result;
}

void play_badapple(void) { //you need flash bad apple ascii 80x25 file to lk2
  #define FRAME_WIDTH 81     //width +1
    #define FRAME_HEIGHT 25     //height
    #define BYTES_PER_LINE (FRAME_WIDTH + 1)  //+1 for \n
    #define FRAME_BYTES (FRAME_HEIGHT * BYTES_PER_LINE)
    static uint8_t raw_frame[FRAME_BYTES];
    static char frame_lines[FRAME_HEIGHT][FRAME_WIDTH + 1];
    long long offset = 0;
    int frame_num = 0;
    const int X_POS = 40;
    const int Y_START = 46;
    const int Y_STEP = 14;
    
    while (1) {
        memset(raw_frame, 0, FRAME_BYTES);
        for (int row = 0; row < FRAME_HEIGHT; row++) {
            memset(frame_lines[row], 0, FRAME_WIDTH + 1);
        }
        ssize_t bytes_read = partition_read("lk2", offset, raw_frame, FRAME_BYTES);
        if (bytes_read < FRAME_BYTES) {
            video_clean_screen();
            video_printf("End at frame %d\n", frame_num);
            mdelay(500);
            show_main_menu();
            mdelay(200);
            break;
        }
        int raw_pos = 0;
        for (int row = 0; row < FRAME_HEIGHT; row++) {
            int line_pos = 0;
            for (int col = 0; col < FRAME_WIDTH; col++) {
                char c = raw_frame[raw_pos++];
                frame_lines[row][line_pos++] = c;
            }
            frame_lines[row][FRAME_WIDTH] = '\0';
            raw_pos++;
        }
        fb_rounded_rect(40, 46, 640, 344, 0, 0);
        fb_set_text_scale(1);
        for (int row = 0; row < FRAME_HEIGHT; row++) {
            fb_text(X_POS, Y_START + (row * Y_STEP), frame_lines[row], FB_WHITE);
        }
        mt_disp_update(0, 0, CONFIG_FRAMEBUFFER_WIDTH, CONFIG_FRAMEBUFFER_HEIGHT);
        if (mtk_detect_key(POWER_KEY)) {
            vendor_show_logo(1);
            mdelay(200);
            show_main_menu();
            break;
        }
        offset += FRAME_BYTES;
        mdelay(18);
    }
}

typedef enum {
    MODE_OFF = 0,
    MODE_ON,
    MODE_FORMAT_DATA,
    GO_BACK,
    SPOOF_MODE_MAX
} spoof_menu_mode_t;

static char spoof_menu_items[SPOOF_MODE_MAX][16] = {
    "OFF",
    "ON",
    "FORMAT DATA",
    "<<MAIN MENU",
};

static void show_lock_spoof_status(void)
{
    const char *val = get_env("lock_spoof");
    fb_set_text_scale(3);
    fb_text(15, 1425, "Lock spoof is currently", OPTIONS_COLOUR);

    if (val && strcmp(val, "enabled") == 0) {
        fb_text(450, 1455, "Enabled", FB_GREEN);
    } else if (val && strcmp(val, "disabled") == 0) {
        fb_text(450, 1455, "Disabled", FB_RED);
    } else {
        fb_text(450, 1455, "Not Set", FB_RED);
    }
}

static void spoof_menu(spoof_menu_mode_t sel) {
    uint32_t start_y = 625;
    uint32_t step = 100;

    video_clean_screen();
    fb_set_text_scale(4);
    fb_text(130, start_y - 140, "bldr spoofing", TITLE_COLOUR);
    for (int i = 0; i < SPOOF_MODE_MAX; ++i) {
        uint32_t y = start_y + i * step;
        if (i == sel) {
            fb_text(120, y, spoof_menu_items[i], FB_GREEN); //selected - green
        } else {
            fb_text(120, y, spoof_menu_items[i], FB_WHITE); //not selected - white
        }
    } 
    fb_set_text_scale(5);
    fb_text(50, 1285, "WARNING !", FB_RED);
    fb_set_text_scale(3);  
    fb_text(15, 1335, "Changing lock spoof", OPTIONS_COLOUR);
    fb_text(15, 1365, "requires format data", OPTIONS_COLOUR);
    fb_text(15, 1395, "Proceed with caution", OPTIONS_COLOUR);
    show_lock_spoof_status();   
    mt_disp_update(0, 0, CONFIG_FRAMEBUFFER_WIDTH, CONFIG_FRAMEBUFFER_HEIGHT);
}

static spoof_menu_mode_t spoof_menu_keypress_loop(void) {
    spoof_menu_mode_t sel = MODE_OFF;
    spoof_menu(sel);
    static int first_run = 1;

    for (;;) {
            if (first_run) {
            first_run = 0;
            sel = SPOOF_MODE_MAX;
            spoof_menu(sel);
            return sel;
        } else if (mtk_detect_key(VOLUME_UP)) {
            mdelay(100);
            sel = (sel == 0) ? (SPOOF_MODE_MAX - 1) : (sel - 1);
            spoof_menu(sel);
        } else if (mtk_detect_key(VOLUME_DOWN)) {
            mdelay(100);
            sel = (sel + 1) % SPOOF_MODE_MAX;
            spoof_menu(sel);
        } else if (mtk_detect_key(POWER_KEY)) {
            mdelay(100); 
            first_run = 1;
            return sel;
        }
        else mdelay(50);         
    }
}

static void set_lock_spoof(int enable) {
    if (enable == 1) {
        set_env("lock_spoof", "enabled");
    } else if (enable == 0) {
        set_env("lock_spoof", "disabled");
    } else {
        set_env("lock_spoof", "not_set");
    }
}

static void format_confirmation(void) {
    vendor_show_logo(3);
    unsigned int wait_time = 0;

    video_clean_screen();
    fb_set_text_scale(3);
    fb_text(30, 1135, "Erasing data in 7 sec", FB_RED);
    fb_text(30, 1165, "Press any key to cancel", FB_RED);
    fb_text(30, 1195, "Pls format from recovery", FB_RED);
    mt_disp_update(0, 0, CONFIG_FRAMEBUFFER_WIDTH, CONFIG_FRAMEBUFFER_HEIGHT);
    while (wait_time < 7000) {
		if (mtk_detect_key(VOLUME_UP) || mtk_detect_key(VOLUME_DOWN) || mtk_detect_key(POWER_KEY)) {
            vendor_show_logo(1);
            mdelay(500);
            return;
        }
		mdelay(100);
		wait_time += 100;
    }
    video_set_cursor(video_get_rows()/2, 0);
    dprintf("Erasing data\n");
    video_printf("Erasing data\n");
    data_wipe();
    dprintf("Erasing data complete\n");
    video_printf("Erasing data complete\n");
    mdelay(5000);
    return;
}

static void spoof_menu_options(spoof_menu_mode_t sel) {
    switch (sel) {
    case MODE_OFF:
        set_lock_spoof(0);
        video_clean_screen();
        vendor_show_logo(3);
        show_spoof_menu();
        break;
    case MODE_ON:
        set_lock_spoof(1);
        video_clean_screen();
        vendor_show_logo(1);
        show_spoof_menu();
        break;
    case MODE_FORMAT_DATA:
        vendor_show_logo(3);
        format_confirmation();
        show_spoof_menu();
        break;
    case GO_BACK:
        mdelay(100);
        vendor_show_logo(1);
        show_main_menu();
        break;
    case SPOOF_MODE_MAX:
        vendor_show_logo(1);
        show_spoof_menu();
        break;
    default:
        break;
    }
}

static void show_spoof_menu(void) {
    spoof_menu_mode_t sel = spoof_menu_keypress_loop();
    spoof_menu_options(sel);
    return;
}

typedef enum {
    MODE_CONTINUE = 0,
    MODE_REBOOT,
    MODE_REBOOT_RECOVERY,
    MODE_REBOOT_BOOTLOADER,
    MODE_POWER_OFF,
    MODE_LOCK_SPOOF,
    BAD_APPLE,
    MODE_MAX
} menu_mode_t;

static char menu_items[MODE_MAX][20] = {
    "CONTINUE",
    "REBOOT",
    "REBOOT RECOVERY",
    "REBOOT BOOTLOADER",
    "POWER OFF",
    "LOCK SPOOF TWEAKS",
    "BAD APPLE"
};

static void main_menu(menu_mode_t sel) {
    uint32_t start_y = 470;
    uint32_t step = 130;  
    fb_set_text_scale(3.5); 
     for (int i = 0; i < MODE_MAX; ++i) {
        uint32_t y = start_y + i * step;
        if (i == sel) {
            fb_text(120, y, menu_items[i], FB_GREEN); //selected - green
        } else {
            fb_text(120, y, menu_items[i], FB_WHITE); //not selected - white
        }
    }  
    mt_disp_update(0, 0, CONFIG_FRAMEBUFFER_WIDTH, CONFIG_FRAMEBUFFER_HEIGHT);
}

static menu_mode_t main_menu_keypress_loop(void) {
    menu_mode_t sel = MODE_CONTINUE;
    main_menu(sel);
    static int first_run = 1;
    for (;;) {
        if (mtk_detect_key(VOLUME_UP)) {
            mdelay(150);
            sel = (sel == 0) ? (MODE_MAX - 1) : (sel - 1);
            main_menu(sel);
        } else if (mtk_detect_key(VOLUME_DOWN)) {
            mdelay(150);
            sel = (sel + 1) % MODE_MAX;
            main_menu(sel);
        } else if (mtk_detect_key(POWER_KEY)) {
            mdelay(150);
            first_run = 1;
            return sel;
        } else if (first_run) {
            first_run = 0;
            sel = MODE_MAX;
            return sel; 
        }
        else mdelay(50);   
    }
}

static void main_menu_options(menu_mode_t sel) {
    switch (sel) {
    case MODE_CONTINUE:
        video_clean_screen();
        mt_disp_show_boot_logo();
        cmd_continue();
        break;
    case MODE_REBOOT:
        cmd_reboot();
        break;
    case MODE_REBOOT_RECOVERY:
        cmd_reboot_recovery();
        break;
    case MODE_REBOOT_BOOTLOADER:
        cmd_reboot_bootloader();
        break;
    case MODE_POWER_OFF:
        mt_power_off();
        break;
    case MODE_LOCK_SPOOF:
        mdelay(100);
        vendor_show_logo(9);
        show_spoof_menu();
        break;
    case BAD_APPLE:
        mdelay(100);
        play_badapple();        
        break;
    case MODE_MAX:
        mdelay(100);
        vendor_show_logo(9);
        show_main_menu();
        break;
    default:
        break;
    }
}

static void show_main_menu(void) {
    menu_mode_t sel = main_menu_keypress_loop();
    main_menu_options(sel);
}

static void spoof_lock_state(void) {
    uint32_t addr = 0;
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB510, 0x4604, 0x2001, 0xF7FF);
    if (addr) {
        printf("Found get_sboot_state at 0x%08X\n", addr);
        PATCH_MEM(addr,
            0x2101,  // movs r1, #1      - set value to 1
            0x6003,  // str r1, [r0,#0]  - store to *param_1
            0x2000,  // movs r0, #0      - return 0
            0x4770   // bx lr            - return
        );
        }
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB1D0, 0xB510, 0x4604, 0xF7FF, 0xFFDD);
    if (addr) {
        printf("Found seccfg_get_lock_state at 0x%08X\n", addr);
        PATCH_MEM(addr + 6, 
            0x2301,  // movs r3, #0x1
            0x6023,  // str r3, [r4, #0x0]
            0x2002,  // movs r0, #0x2
            0xbd10   // pop {r4, pc}
        );
        }
    PATCH_MEM(FASTBOOT_HNDLR1, 0xE008);
    PATCH_MEM(FASTBOOT_HNDLR2, 0xE78C);
    dprintf("fastboot_handler Patched\n");

    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF75, 0xF3C0);
    if (addr) {
        printf("Found get_vfy_policy at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF6F, 0xF000);
    if (addr) {
        printf("Found get_dl_policy at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4FF0, 0xB0A9, 0xF101);
    if (addr) {
        printf("Found AVB cmdline function at 0x%08X\n", addr);
        NOP(addr + 0x9C, 4);
    }
}

static void check_lock_spoof(void) {
    const char *val = get_env("lock_spoof");

    if (val && strcmp(val, "enabled") == 0) {
        spoof_lock_state();
        dprintf("Lock spoof is Enabled\n");
    } else if (val && strcmp(val, "disabled") == 0) {
        dprintf("Lock spoof is Disabled\n");
    } else if (val && strcmp(val, "not_set") == 0) {
        dprintf("Lock spoof is not_set\n");
    } else {
        dprintf("Something went wrong with kaeru lock spoof environment\n");
        dprintf("or possible first boot of kaeru detected\n");
        dprintf("Setting lock_spoof as not_set\n");
        set_env("lock_spoof", "not_set");
    }
}

static void hijack_env_init(void) {
    env_init();
    check_lock_spoof();
}

static int menu_thread(void *arg) {
    video_clean_screen();
    mtk_wdt_disable();
    video_set_cursor(video_get_rows()/2, 0);           
    dprintf("menu loaded\n");
    show_main_menu();
	return 0;
}

void basic_patches(void){
    uint32_t addr = 0;
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0x4B0E, 0x447B, 0x681B);
    if (addr) {
        printf("Found orange_state_warning at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }
   addr = SEARCH_PATTERN(LK_START, LK_END, 0xB530, 0xB083, 0xAB02, 0x2200, 0x4604);
    if (addr) {
        printf("Found dm_verity_corruption at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4FF0, 0xB0A9, 0xF101);
    if (addr) {
        printf("Found AVB cmdline function at 0x%08X\n", addr);
        NOP(addr + 0x9C, 4);
    }
    // LK has two security gates in the fastboot command processor that
    // reject commands with "not support on security" and "not allowed
    // in locked state" errors. When spoofing lock state, these would
    // block all fastboot operations despite the device being actually
    // unlocked underneath.
    //
    // Even without spoofing, we patch these out as a safety measure
    // since OEM-specific checks could still interfere with fastboot
    // commands in unexpected ways.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4880, 0xB087, 0x4D5A);
    if (addr) {
        printf("Found fastboot command processor at 0x%08X\n", addr);
    
        // "not support on security" call
        NOP(addr + 0x15A, 2);

        // "not allowed in locked state" call
        NOP(addr + 0x166, 2);
    
        // Jump directly to command handler
        PATCH_MEM(addr + 0xF0, 0xE006);
    }
}

static void thread_inject(void) {
    thread_t *thr;
    thr = thread_create("menu", menu_thread, NULL, LOW_PRIORITY, DEFAULT_STACK_SIZE);
    thread_resume(thr);
	return;
}

static void register_fastboot_commands(void) {
    fastboot_register("oem badapple", play_badapple, 0);
    dprintf("Fastboot framebuffer commands registered\n");
}

void board_early_init(void) {
    printf("Entering early init for Realme c15\n");
    basic_patches();
    PATCH_CALL(ENV_INIT_CALL, (void*)hijack_env_init, TARGET_THUMB);
    register_fastboot_commands();
}

void board_late_init(void) {
    dprintf("Entering late init for Realme c15\n");

    if (get_bootmode() == BOOTMODE_FASTBOOT) {
        thread_inject(); 
    } 
    if (mtk_detect_key(VOLUME_DOWN)) {
        cmd_reboot_bootloader();
    } 
}
