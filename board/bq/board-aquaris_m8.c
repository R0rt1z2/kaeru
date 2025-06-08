//
// SPDX-FileCopyrightText: 2025 R0rt1z2 <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define VOLUME_DOWN 5

void board_early_init(void) {
    printf("Entering early init for BQ Aquaris M8\n");
    
    // These two functions are used to determine whether the device is secure or not.    
    // Depending on the SBC eFuse, they will return either 0 (not secure) or 1 (secure).  
    //                                                                                   
    // Many fastboot commands are blocked by these checks, so we override them to always  
    // return 0 (not secure) regardless of the actual hardware security configuration.    
    FORCE_RETURN(0x41E2716C, 0);                                                          
    FORCE_RETURN(0x41E29058, 0);

    // This function checks whether a partition can be flashed. If flashing is not        
    // allowed, it sets the second parameter to 0x1, which is later checked by the
    // caller.
    //                                                                                    
    // This patch forces the second parameter to 0x0, allowing all partitions to be       
    // flashed regardless of the current security state.                                  
    PATCH_MEM(0x41E289B8,                                                                 
                0x2001,  // movs r1, #1                                                   
                0x6008,  // str r1, [r1, #0x0]                                             
                0x2000,  // movs r0, #0                                                   
                0x4770   // bx lr                                                         
    );
}

void board_late_init(void) {
    printf("Entering late init for BQ Aquaris M8\n");

    // There is no easy or direct way to enter fastboot mode on this device, so we        
    // use the volume buttons to detect key presses and manually set the boot mode.       
    if (mtk_detect_key(VOLUME_DOWN)) {                                                   
        set_bootmode(BOOTMODE_FASTBOOT);              
        show_bootmode(BOOTMODE_FASTBOOT);                                  
    }

    // Suppresses the bootloader unlock warning shown during boot on
    // unlocked devices. In addition to the visual warning, it also
    // introduces an unnecessary 5-second delay.
    // 
    // This patch get rid of the delay and the warning by forcing the
    // function that holds the logic to always return 0 and therefore
    // not executing the code that shows the warning.
    FORCE_RETURN(0x41E03A48, 0);
}
