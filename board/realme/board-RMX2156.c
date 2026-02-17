//
// SPDX-FileCopyrightText: 2025 R0rt1z2 <me@r0rt1z2.com>
// SPDX-FileCopyrightText: 2025 antagonizzzt <antagonizzzt@gmail.com>
// SPDX-FileCopyrightText: 2025 Shomy 🍂 <shomy@shomy.is-a.dev>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>
#include <lib/thread.h>
#include <wdt/mtk_wdt.h>

#define VOLUME_UP 17
#define VOLUME_DOWN 1
#define POWER_KEY 8

#define BG_COLOUR FB_TEAL
#define TITLE_COLOUR FB_LIME
#define OPTIONS_COLOUR FB_WHITE
#define ROUND_RECT_COLOUR FB_ORANGE
#define ROUND_RECT_RADIUS 8

#ifdef CONFIG_REALME_RMX2156_UI2 // Realme UI2 C18
// calls
#define CLEAN_SCREEN            0x4C431DC0
#define GET_ROWS                0x4C431DB4
#define GET_COLUMNS             0x4C5277A0     
#define SET_CURSOR              0x4C431D78
#define DRINTF_ADDR             0x4C432040
#define SHOW_LOGO_ADDR          0x4C405364
#define MDELAY_ADDR             0x4C451D48
#define CONTINUE_ADDR           0x4C427024
#define REBOOT_RECOVERY_ADDR    0x4C427260
#define POWER_OFF_ADDR          0x4C4187B4
#define REBOOT_BL_ADDR          0x4C4276F8
#define REBOOT_ADDR             0x4C4276C8
#define DISP_UPDATE_ADDR        0x4C4014F8
// basic patches
#define VERIFY_OK               0x4C46768C
#define ORANGE_STATE            0x4C444214
#define DM_VERITY               0x4C458E60
#define FIVE_SEC_DELAY          0x4C427070  // removed unnecessary 5 sec delay in cmd_continue
#define FASTBOOT_INIT1          0x4C4264DC  // video_clean_screen
#define FASTBOOT_INIT2          0x4C4264E0  // video_get_rows
#define FASTBOOT_INIT3          0x4C4264EE  // video_set_cursor
#define FASTBOOT_INIT4          0x4C4264F8  // video_printf("[Fastboot    Mode] \n");
// lock spoof related patches
#define LIB_SEC_ADDR            0x4C46B418
#define LOCK_STATE_JUMP         0x4C46C116
#define FASTBOOT_HNDLR1         0x4C4261F0
#define FASTBOOT_HNDLR2         0x4C42621E
#define VFY_POLICY_ADDR         0x4C4154A0
#define DL_POLICY_ADDR          0x4C4154AC
#define AVB_APND_OPTS1          0x4C4532E2
#define AVB_APND_OPTS2          0x4C4532E6
#define ENV_INIT_CALL           0x4C40344E
#define ENV_INIT_ADDR           0x4C44BAD8
#define SET_ENV_ADDR            0x4C44BF50
#define GET_ENV_ADDR            0x4C44BCCC // get_env_with_area
#define DATA_WIPE_ADDR          0x4C42E6B4 // use it with caution
// End of RUI2 patches
#else // Realme UI3 F18 by default
// calls
#define CLEAN_SCREEN            0x4C436508
#define GET_ROWS                0x4C4364FC
#define GET_COLUMNS             0x4C543D64      
#define SET_CURSOR              0x4C4364C0
#define DRINTF_ADDR             0x4C436788
#define SHOW_LOGO_ADDR          0x4C405650
#define MDELAY_ADDR             0x4C4587B0
#define CONTINUE_ADDR           0x4C429B30
#define REBOOT_RECOVERY_ADDR    0x4C429D6C
#define POWER_OFF_ADDR          0x4C418E10
#define REBOOT_BL_ADDR          0x4C42A204
#define REBOOT_ADDR             0x4C42A1D4
#define DISP_UPDATE_ADDR        0x4C40167C
// basic patches
#define VERIFY_OK               0x4C46D460
#define ORANGE_STATE            0x4C449150
#define DM_VERITY               0x4C45F930
#define FIVE_SEC_DELAY          0x4C429B7C  // removed unnecessary 5 sec delay in cmd_continue
#define FASTBOOT_INIT1          0x4C429010  // video_clean_screen
#define FASTBOOT_INIT2          0x4C429014  // video_get_rows
#define FASTBOOT_INIT3          0x4C429022  // video_set_cursor
#define FASTBOOT_INIT4          0x4C42902C  // video_printf("[Fastboot    Mode] \n");
// lock spoof related patches
#define LIB_SEC_ADDR            0x4C46FAE4
#define LOCK_STATE_JUMP         0x4C4709AE
#define FASTBOOT_HNDLR1         0x4C428D14
#define FASTBOOT_HNDLR2         0x4C428D42
#define VFY_POLICY_ADDR         0x4C415A1C
#define DL_POLICY_ADDR          0x4C415A28
#define AVB_APND_OPTS1          0x4C459D4E
#define AVB_APND_OPTS2          0x4C459D52
#define ENV_INIT_CALL           0x4C403748
#define ENV_INIT_ADDR           0x4C450F2C
#define SET_ENV_ADDR            0x4C451338
#define GET_ENV_ADDR            0x4C451120 // get_env_with_area
#define DATA_WIPE_ADDR          0x4C432864 // use it with caution
// End of RUI3 patches
#endif

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

static void mt_disp_update(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    ((void (*)(uint32_t, uint32_t, uint32_t, uint32_t))(DISP_UPDATE_ADDR | 1))(x, y, width, height);
}

static void env_init(void) {
    ((void (*)(void))(ENV_INIT_ADDR | 1))();
}

static int set_env(char *name, char *value) {
    return ((int (*)(char *, char *))(SET_ENV_ADDR | 1))(name, value);
}

static char *get_env(char *name) {
    return ((char *(*)(char *))(GET_ENV_ADDR | 1))(name);
}

static void data_wipe(void) {
    ((void (*)(void))(DATA_WIPE_ADDR | 1))();
}

#ifdef CONFIG_REALME_RMX2156_UI2
static void patch_ui2(void) {
    dprintf("PATCH_UI2: No RUI2 specific patches for now...\n");
}
#else
static void patch_ui3(void) {

    dprintf("Applying RUI3 specific patches...\n");

    // Branching the double_check_lock_state flow to end just before it triggers the force lock
    // function fastboot_oem_lock_chk(1). We can also force return this function. But it 
    // produces some useful logs related to rpmb_lock_state during the first boot and 
    // that might be useful for debugging
    PATCH_MEM(0x4C46DAE0, 0xE00B);
    NOP(0x4C46DAE2, 2); // Extra safety
    dprintf("PATCH_UI3: double_check_lock_state Patched despite lock state\n");
    
}
#endif

static void show_main_menu(void);
static void show_spoof_menu(void);

typedef enum {
    MODE_NO = 0,
    MODE_YES,
    MODE_FORMAT_DATA,
    GO_BACK,
    SPOOF_MODE_MAX
} spoof_menu_mode_t;

static char spoof_menu_items[SPOOF_MODE_MAX][16] = {
    "NOPE",
    "YUP",
    "FORMAT DATA",
    "<< BACK",
};

static void show_lock_spoof_status(void)
{
    const char *val = get_env("lock_spoof");
    fb_set_text_scale(3);
    fb_text(20, 1800, "Lock spoof is currently", OPTIONS_COLOUR);

    if (val && strcmp(val, "enabled") == 0) {
        fb_text(600, 1800, "Enabled", FB_GREEN);
    } else if (val && strcmp(val, "disabled") == 0) {
        fb_text(600, 1800, "Disabled", FB_RED);
    } else {
        fb_text(600, 1800, "Not Set", FB_RED);
    }
}

static void spoof_menu(spoof_menu_mode_t sel)
{
    uint32_t start_y = 700;
    uint32_t step = 200;  // Line Spacing

    video_clean_screen(); // Cleans video_printf texts while changing options
    fb_clear(BG_COLOUR);  // Sets Background 
    fb_set_text_scale(7); // Title size
    fb_text(90, start_y - 220, "WANT LOCK SPOOF?", TITLE_COLOUR); // Title
  
    fb_set_text_scale(5); // Options' size  
    for (int i = 0; i < SPOOF_MODE_MAX; ++i) {
        uint32_t y = start_y + i * step;
        if (i == sel) {
            fb_rounded_rect(60, y - 60, CONFIG_FRAMEBUFFER_WIDTH - 120, 160, ROUND_RECT_RADIUS, ROUND_RECT_COLOUR);
        }
        fb_text(120, y, spoof_menu_items[i], OPTIONS_COLOUR);
    } 

    fb_set_text_scale(6);
    fb_text(30, 1625, "WARNING !", FB_RED);
    fb_set_text_scale(3);  
    fb_text(20, 1700, "Changing lock spoof requires format data", OPTIONS_COLOUR);
    fb_text(20, 1750, "Proceed with caution", OPTIONS_COLOUR);

    show_lock_spoof_status();
    
    mt_disp_update(0, 0, CONFIG_FRAMEBUFFER_WIDTH, CONFIG_FRAMEBUFFER_HEIGHT);
}

static spoof_menu_mode_t spoof_menu_keypress_loop(void)
{
    spoof_menu_mode_t sel = MODE_NO;
    spoof_menu(sel);
    for (;;) {
        if (mtk_detect_key(VOLUME_UP)) {
            mdelay(100);
            sel = (sel == 0) ? (SPOOF_MODE_MAX - 1) : (sel - 1);
            spoof_menu(sel);
        } else if (mtk_detect_key(VOLUME_DOWN)) {
            mdelay(100);
            sel = (sel + 1) % SPOOF_MODE_MAX;
            spoof_menu(sel);
        } else if (mtk_detect_key(POWER_KEY)) {
            mdelay(100);
            fb_clear(BG_COLOUR); // Clean framebuffer before executing
            return sel;
        }
        else mdelay(50);         // Prevents cpu overuse
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

    unsigned int wait_time = 0;

    video_clean_screen();
    fb_clear(BG_COLOUR);
    fb_set_text_scale(3);
    fb_text(40, 1700, "Erasing data in 15 sec", FB_RED);
    fb_text(40, 1750, "Press any key to cancel", FB_RED);
    mt_disp_update(0, 0, CONFIG_FRAMEBUFFER_WIDTH, CONFIG_FRAMEBUFFER_HEIGHT);

    while (wait_time < 15000) {
		if (mtk_detect_key(VOLUME_UP) || mtk_detect_key(VOLUME_DOWN) || mtk_detect_key(POWER_KEY)) {
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

static void spoof_menu_options(spoof_menu_mode_t sel)
{
    switch (sel) {
    case MODE_NO:
        set_lock_spoof(0);
        video_clean_screen();
        fb_clear(BG_COLOUR);
        show_spoof_menu();
        break;
    case MODE_YES:
        set_lock_spoof(1);
        video_clean_screen();
        fb_clear(BG_COLOUR);
        show_spoof_menu();
        break;
    case MODE_FORMAT_DATA:
        format_confirmation();
        show_spoof_menu();
        break;
    case GO_BACK:
        mdelay(100);
        show_main_menu();
        break;
    default:
        break;
    }
}

static void show_spoof_menu(void) {
    spoof_menu_mode_t sel = spoof_menu_keypress_loop();
    spoof_menu_options(sel);
}

typedef enum {
    MODE_CONTINUE = 0,
    MODE_REBOOT,
    MODE_REBOOT_RECOVERY,
    MODE_REBOOT_BOOTLOADER,
    MODE_POWER_OFF,
    MODE_LOCK_SPOOF,
    MODE_CUSTOM_FUNC,
    MODE_MAX
} menu_mode_t;

static char menu_items[MODE_MAX][20] = {
    "CONTINUE",
    "REBOOT",
    "REBOOT RECOVERY",
    "REBOOT BOOTLOADER",
    "POWER OFF",
    "LOCK SPOOF TWEAKS",
    "CUSTOM FUNCTION"
};

static void main_menu(menu_mode_t sel)
{
    uint32_t start_y = 700;
    uint32_t step = 200;  // Line Spacing

    video_clean_screen(); // Cleans video_printf texts while changing options
    fb_clear(BG_COLOUR);  // Sets Background 
    fb_set_text_scale(8); // Title size
    fb_text(120, start_y - 220, "FASTBOOT MODE", TITLE_COLOUR); // Title
  
    fb_set_text_scale(5); // Options' size  
    for (int i = 0; i < MODE_MAX; ++i) {
        uint32_t y = start_y + i * step;
        if (i == sel) {
            fb_rounded_rect(60, y - 60, CONFIG_FRAMEBUFFER_WIDTH - 120, 160, ROUND_RECT_RADIUS, ROUND_RECT_COLOUR);
        }
        fb_text(120, y, menu_items[i], OPTIONS_COLOUR);
    }  
    mt_disp_update(0, 0, CONFIG_FRAMEBUFFER_WIDTH, CONFIG_FRAMEBUFFER_HEIGHT);
}

static menu_mode_t main_menu_keypress_loop(void)
{
    menu_mode_t sel = MODE_CONTINUE;
    main_menu(sel);
    for (;;) {
        if (mtk_detect_key(VOLUME_UP)) {
            mdelay(100);
            sel = (sel == 0) ? (MODE_MAX - 1) : (sel - 1);
            main_menu(sel);
        } else if (mtk_detect_key(VOLUME_DOWN)) {
            mdelay(100);
            sel = (sel + 1) % MODE_MAX;
            main_menu(sel);
        } else if (mtk_detect_key(POWER_KEY)) {
            mdelay(100);
            fb_clear(BG_COLOUR); // Clean framebuffer before executing
            return sel;
        }
        else mdelay(50);         // Prevents cpu overuse
    }
}

static void main_menu_options(menu_mode_t sel)
{
    switch (sel) {
    case MODE_CONTINUE:
        video_clean_screen();
        mt_disp_show_boot_logo();
        cmd_continue();
        break;
    case MODE_REBOOT:
        video_clean_screen();
	    video_set_cursor(video_get_rows()/2, 0);
        video_printf("Rebooting... Please Wait\n");
        mdelay(1500);
        cmd_reboot();
        break;
    case MODE_REBOOT_RECOVERY:
        video_clean_screen();
	    video_set_cursor(video_get_rows()/2, 0);
        video_printf("Rebooting to Recovery... Please Wait\n");
        mdelay(1500);
        cmd_reboot_recovery();
        break;
    case MODE_REBOOT_BOOTLOADER:
        video_clean_screen();
	    video_set_cursor(video_get_rows()/2, 0);
        video_printf("Rebooting to Bootloader... Please Wait\n");
        mdelay(1500);
        cmd_reboot_bootloader();
        break;
    case MODE_POWER_OFF:
        video_clean_screen();
	    video_set_cursor(video_get_rows()/2, 0);
        video_printf("Turning OFF... Please Wait\n");
        mdelay(1500);
        mt_power_off();
        break;
    case MODE_LOCK_SPOOF:
        mdelay(100);
        show_spoof_menu();
        break;
    case MODE_CUSTOM_FUNC:
        video_clean_screen();
	    video_set_cursor(video_get_rows()/2, 0);
        video_printf("Coming Soon... Reserved for ATF and some other stuff\n");
        video_printf("Rebooting to bootloader for now ! \n");
        mdelay(3500);
        cmd_reboot_bootloader();
        break;
    default:
        break;
    }
}

static void show_main_menu(void)
{
    menu_mode_t sel = main_menu_keypress_loop();
    main_menu_options(sel);
}

static void spoof_lock_state(void) {

    // Patching seclib_sec_boot_enabled to always return 1 does the job needed for get_sboot_state
    FORCE_RETURN(LIB_SEC_ADDR, 1);
    dprintf("get_sboot_state Patched\n");

    // Branching to the required codes in get_lock_state always returns the locked state
    PATCH_MEM(LOCK_STATE_JUMP, 0xE013);
    dprintf("get_lock_state Patched\n");
    
    // Branching to the required codes in order to skip security checks for fastboot commands
    PATCH_MEM(FASTBOOT_HNDLR1, 0xE008);
    PATCH_MEM(FASTBOOT_HNDLR2, 0xE78C);
    dprintf("fastboot_handler Patched\n");

    // Patching get_vfy_policy to always return 0 to skip verification/auth
    FORCE_RETURN(VFY_POLICY_ADDR, 0);
    dprintf("get_vfy_policy Patched\n");

    // Patching dl_policy to always return 0 to remove flash restrictions
    FORCE_RETURN(DL_POLICY_ADDR, 0);
    dprintf("dl_policy Patched\n");

    // NOP-ing some branches will give locked state always
    NOP(AVB_APND_OPTS1, 1);
    NOP(AVB_APND_OPTS2, 1);
    dprintf("avb_append_options Patched\n");

}

static void check_lock_spoof(void)
{
    const char *val = get_env("lock_spoof");

    if (val && strcmp(val, "enabled") == 0) {
        spoof_lock_state();
        dprintf("Lock spoof is Enabled\n");
    } else if (val && strcmp(val, "disabled") == 0) {
        dprintf("Lock spoof is Disabled\n");
    } else if (val && strcmp(val, "not_set") == 0) {
        dprintf("Lock spoof is not_set\n");
    } else {
        dprintf("Something went wrong with kaeru lock spoof environment...\n");
        dprintf("or possible first boot of kaeru detected...\n");
        dprintf("Setting lock_spoof as not_set\n");
        set_env("lock_spoof", "not_set");
    }
}

static void hijack_env_init(void) {
    env_init();
    check_lock_spoof();
}

static int menu_thread(void *arg)
{
    mtk_wdt_disable();         // Prevents wdt_reset
    mt_disp_show_boot_logo();  // Calling mt_disp_show_boot_logo here seems to be uselss but it initializes 
                               // display and other framebuffer related things that prevents black screen issues
    video_set_cursor(video_get_rows()/2, 0);
    video_printf("Loading Menu... Please Wait\n");
    mdelay(1500);              // delay to avoid any accidental key presses
                               // and to let the fastboot to settle
    dprintf("Fastboot menu loaded...\n");
    show_main_menu();
	return 0;
}

static void thread_inject(void) {
    thread_t *thr;
#ifdef CONFIG_THREAD_SUPPORT
    thr = thread_create("antagonizzzt", menu_thread, NULL, LOW_PRIORITY, DEFAULT_STACK_SIZE);
    if (!thr)
    {
        video_printf("Can't create thread and you are in Fastboot mode now !\n");
        return;
    }
    thread_resume(thr);
#else
    mt_disp_show_boot_logo();
    video_set_cursor(video_get_rows()/2, 0);
    video_printf("Loading... Please Wait\n");
    mdelay(1500);
    dprintf("Thread skipped.\n");
    video_printf("Thread skipped and you are in Fastboot mode now !\n");
#endif
	return;
}

void basic_patches(void){

    FORCE_RETURN(VERIFY_OK, 0);
    dprintf("unlock_verify_ok Patched\n");

    FORCE_RETURN(ORANGE_STATE, 0);
    dprintf("Orange_State Patched\n");

    // not patching red state because it'll show up if the device has any serious issues
    // like "not getting the device state" etc 

    FORCE_RETURN(DM_VERITY, 0);
    dprintf("dm_verity_handler Patched\n");

    PATCH_MEM(FIVE_SEC_DELAY, 0xE7E7);
    dprintf("cmd_continue Patched\n");

    // Skip some funtions in Fastboot_init that clashes with that of custom menu.
    // If the thread loads the boot menu earlier, the video_clean_screen in Fastboot_init will
    // clean the menu while entering Fastboot_init
    NOP(FASTBOOT_INIT1, 2);
    NOP(FASTBOOT_INIT2, 2);
    NOP(FASTBOOT_INIT3, 2);
    NOP(FASTBOOT_INIT4, 2);
    dprintf("fastboot_init Patched\n");
}

void board_early_init(void) {

    dprintf("Entering early init for Realme Narzo 30 4G\n");

    basic_patches();

#ifdef CONFIG_REALME_RMX2156_UI2
    patch_ui2();
#else  // RUI3 by default
    patch_ui3();
#endif

    // Patch calling env_init to inject check_lock_spoof() right after 
    // manual initialization of environment
    PATCH_CALL(ENV_INIT_CALL, (void*)hijack_env_init, TARGET_THUMB);
    
    // - Volume Up → Recovery
    // - Volume Down → Fastboot
    if (mtk_detect_key(VOLUME_UP)) {
        set_bootmode(BOOTMODE_RECOVERY);
    } else if (mtk_detect_key(VOLUME_DOWN)) {
        set_bootmode(BOOTMODE_FASTBOOT);
    }
}

void board_late_init(void) {

    dprintf("Entering late init for Realme Narzo 30 4G\n");

    if (get_bootmode() == BOOTMODE_FASTBOOT) {
        thread_inject(); // The thread will run along with fastboot mode
    } 
}

