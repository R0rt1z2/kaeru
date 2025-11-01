//
// SPDX-FileCopyrightText: 2025 R0rt1z2 <me@r0rt1z2.com>
// SPDX-FileCopyrightText: 2025 antagonizzzt <antagonizzzt@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>
#include <lib/thread.h>
#include <wdt/mtk_wdt.h>

#define VOLUME_UP 17
#define VOLUME_DOWN 1
#define POWER_KEY 8

static void video_clean_screen(void) {
    ((void (*)(void))(0x4C436508 | 1))();
}

static int video_get_rows(void) {
    return ((int (*)(void))(0x4C4364FC | 1))();
}

static void video_set_cursor(int row, int col) {
    ((void (*)(int, int))(0x4C4364C0 | 1))(row, col);
}

static int dprintf(const char* fmt, ...) {
    return ((int (*)(const char*))(0x4C436788 | 1))(fmt);
}

static void mt_disp_show_boot_logo(void) {
    ((void (*)(void))(0x4C405650 | 1))();
}

static void mdelay(unsigned long msecs) {
    ((void (*)(unsigned long))(0x4C4587B0 | 1))(msecs);
}

static void cmd_continue(void) {
    ((void (*)(void))(0x4C429B30 | 1))();
}

static void cmd_reboot_recovery(void) {
    ((void (*)(void))(0x4C429D6C | 1))();
}

static void mt_power_off(void) {
    ((void (*)(void))(0x4C418E10 | 1))();
}

// A basic menu for now. We can make it fancy in the future. Maybe an
// advanced menu that can be interacted on each keypress or some custom 
// splash images for each modes (normal/recovery/power off/etc) by
// calling vendor_show_logo function which is used to show any image
// from logo.bin
static void show_basic_menu(void)
{
	video_clean_screen(); // Cleans the text on the screen if any
	video_set_cursor(video_get_rows()/2, 0); // Mid-left
	video_printf("Aah Shit here we go again :0\n");
	video_printf("------------------------------\n");
	video_printf("Volume UP   - Boot into System\n");
	video_printf("Volume Down - Boot to Recovery\n");
	video_printf("Power Key   - Turn OFF Device\n");
	video_printf("------------------------------");
}

static int my_thread(void *arg)
{
    mtk_wdt_disable();          // Prevents wdt_reset
    mt_disp_show_boot_logo();   // instead of black screen
    show_basic_menu();
    mdelay(2500);               // 2.5 sec delay to avoid any accidental key presses
                                // and to let the fastboot to settle
	for (;;) {
        if (mtk_detect_key(VOLUME_UP)) {
            video_clean_screen();
            video_set_cursor(video_get_rows()/2, 0);
            video_printf("Booting to system... Please Wait\n");
            cmd_continue();
        } else if (mtk_detect_key(VOLUME_DOWN)) {
            cmd_reboot_recovery();
        } else if (mtk_detect_key(POWER_KEY)) {
            mt_power_off();     // aka mt_power_off_new	
        }
        else mdelay(25);        // Prevents cpu overuse
	}
	return 0;
}

static void thread_inject(void) {

    thread_t *thr;

#ifdef CONFIG_THREAD_SUPPORT
    thr = thread_create("antagonizzzt", my_thread, NULL, DEFAULT_PRIORITY, DEFAULT_STACK_SIZE);
    if (!thr)
    {
         video_printf("Can't create thread and you are in Fastboot mode now !\n");
         return;
    }
    thread_resume(thr);
#else
    dprintf("Thread skipped.\n");
    video_printf("Thread skipped and you are in Fastboot mode now !\n");
#endif
	return;
}

void board_early_init(void) {
    
    dprintf("Entering early init for Realme Narzo 30 4G\n");

    FORCE_RETURN(0x4C46D460, 0);
    dprintf("unlock_verify_ok Patched\n");

    FORCE_RETURN(0x4C449150, 0);
    dprintf("Orange_State Patched\n");

    // not patching red state because it'll show up if the device has any serious issues
    // like "not getting the device state" etc 

    FORCE_RETURN(0x4C45F930, 0);
    dprintf("dm_verity_handler Patched\n");

#ifdef CONFIG_FORCE_LOCK_SPOOF

    // Branching the double_check_lock_state flow to end just before it triggers the force lock
    // function fastboot_oem_lock_chk(1). We can also force return this fumction.
    // But this function produces some useful logs related to rpmb_lock_state during the
    // first boot and that might be useful for debugging
    
    PATCH_MEM(0x4C46DAE0, 0xE00B);
    NOP(0x4C46DAE2, 2); // Extra safety
    dprintf("double_check_lock_state Patched\n");

    // Patching seclib_sec_boot_enabled to always return 1 does the job needed for get_sboot_state
    FORCE_RETURN(0x4C46FAE4, 1);
    dprintf("get_sboot_state Patched\n");

    // Branching to the required codes in get_lock_state always returns the locked state
    PATCH_MEM(0x4C4709AE, 0xE013);
    dprintf("get_lock_state Patched\n");
    
    // Branching to the required codes in order to skip security checks for fastboot commands
    PATCH_MEM(0x4C428D14, 0xE008);
    PATCH_MEM(0x4C428D42, 0xE78C);
    dprintf("fastboot_handler Patched\n");

    // Patching get_vfy_policy to always return 0 to skip verification/auth
    FORCE_RETURN(0x4C415A1C, 0);
    dprintf("get_vfy_policy Patched\n");

    // Patching dl_policy to always return 0 to remove flash restrictions
    FORCE_RETURN(0x4C415A28, 0);
    dprintf("dl_policy Patched\n");

    // NOP-ing some branches will give locked state always
    NOP(0x4C459D4E, 1);
    NOP(0x4C459D52, 1);
    dprintf("avb_append_options Patched\n");

#endif

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

