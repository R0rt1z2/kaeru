//
// board-w7.c — LG W7 (MT6765) — template para kaeru
// Baseado em board-mcaymanlm.c (LG Velvet), com os pontos device-específicos
// do W7 resolvidos a partir do lk_a.bin (ver defconfig_corrigido.txt §2).
//
// Instalação (vendor "LGE" → dir lge/ — sanitize_vendor_path = to_lower):
//   ./setup.sh -c w7 -m "LG W7" -v LGE -l <lk> -s MT6765  cria automaticamente:
//     board/lge/Kconfig   -> entrada `config LGE_W7`
//     board/lge/Makefile  -> lib-$(CONFIG_LGE_W7) += lge/board-w7.o
//     board/lge/board-w7.c (stub) -> SUBSTITUÍDO por este arquivo
//     configs/lge/w7_defconfig   -> SUBSTITUÍDO pelo w7_defconfig verificado
//

#include <board_ops.h>

#include <wdt/mtk_wdt.h>

// Mesmo código do mcaymanlm; se o keymap do W7 for outro, ajustar
// (mtk_detect_key do W7 aceita 0..0x47, com caminhos especiais p/ 8 e 0x11).
#define VOLUME_UP 1

long partition_read(const char* part_name, long long offset, uint8_t* data, size_t size) {
    return ((long (*)(const char*, long long, uint8_t*, size_t))(CONFIG_PARTITION_READ_ADDRESS | 1))(
            part_name, offset, data, size);
}

// Irmã gêmea de partition_read no LK do W7 (0x48053C54): mesmo prólogo e
// mesmos helpers; só o storage op final muda (0x4806C9B4 = write).
// Os 8 primeiros halfwords são idênticos aos do read — o 9º (0xFDEF) desambigua.
long partition_write(const char* part_name, long long offset, uint8_t* data, size_t size) {
    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4FF0, 0xB085, 0x461F, 0x4616,
                                                     0x9003, 0x9D0F, 0xF7FE, 0xFDEF);
    if (addr)
        return ((long (*)(const char*, long long, uint8_t*, size_t))(addr | 1))(
                part_name, offset, data, size);
    return -1;
}

// OPCIONAL (padrão do mcaymanlm NÃO existe no binário do W7): se o W7 também
// só alcança o fastboot stock via ramdump, achar o branch equivalente no
// lk_a.bin do W7 (procurar a checagem da flag de mrdump perto da escolha de
// boot path do LG) e NOPar. A UI do kaeru não depende disso.
// static void force_fastboot(void) {
//     uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, /* pattern do W7 */);
//     if (addr) {
//         NOP(addr + 4, 1);
//         show_bootmode(BOOTMODE_FASTBOOT);
//     }
// }

// O LK stock ignora boot commands na misc — parseamos aqui e deixamos as
// ferramentas escolherem o boot mode.
static bool parse_bootloader_messages(void) {
    struct misc_message misc_msg = {0};

    if (partition_read("misc", 0, (uint8_t*)&misc_msg, sizeof(misc_msg)) < 0) {
        printf("Failed to read misc partition\n");
        return false;
    }

    bootmode_t mode = misc_command_to_bootmode(misc_msg.command);
    if (mode == BOOTMODE_NORMAL)
        return false;

    printf("Found '%s', forcing %s\n", misc_msg.command, bootmode2str(mode));

    if (!misc_command_is_sticky(misc_msg.command) &&
        !misc_command_is_for_ramdisk(misc_msg.command)) {
        memset(misc_msg.command, 0, sizeof(misc_msg.command));
        partition_write("misc", 0, (uint8_t*)&misc_msg, sizeof(misc_msg));
    }

    set_bootmode(mode);
    show_bootmode(mode);

    return false;
}

static void cmd_reboot(const char* arg, void* data, unsigned sz) {
    const char* command = reboot_target_to_misc_command(arg);

    if (!command && arg && arg[0]) {
        fastboot_fail("unknown reboot target");
        return;
    }

    if (command) {
        struct misc_message misc_msg = {0};

        strncpy(misc_msg.command, command, sizeof(misc_msg.command) - 1);

        if (partition_write("misc", 0, (uint8_t*)&misc_msg, sizeof(misc_msg)) < 0) {
            fastboot_fail("failed to write misc");
            return;
        }
    }

    fastboot_okay("");
    mtk_wdt_reset();
}

void board_early_init(void) {
    printf("Entering early init for LG W7\n");
}

void board_late_init(void) {
    printf("Entering late init for LG W7\n");

    uint32_t addr = 0;

    // CAUSA RAIZ do fastboot que "vira Android": o dispatcher de boot mode
    // stock (0x4802E9FC, chamado por mt_boot em 0x4802F554) NÃO tem case para
    // 99 (FASTBOOT). O default em 0x4802EBEE (bne.w 0x4802EADA) segue para o
    // boot comum do Android (append "androidboot.force_normal_boot=1" e jump
    // ao kernel em 0x4802CCD0, noreturn). O fastboot_init (0x48030A20) só roda
    // se o dispatcher RETORNAR — e o único epílogo é 0x4802EBDA (movs r0,#0;
    // add sp,#0x9c; pop.w {r4-r11,pc}), alcançado só se o boot do kernel
    // falhar. Daí o sintoma: overlay do kaeru some em ~1s e o Android sobe.
    //
    // Patch cirúrgico (byte a byte verificado com capstone) no fim da cadeia
    // mode>5 — r3 ainda é g_boot_mode (ldr r3,[r6] em 0x4802EA66, preservado
    // até 0x4802EBEC):
    //   0x4802EBEC  cmp r3,#0xc8   ->  cmp r3,#99
    //   0x4802EBEE  bne.w default  ->  beq 0x4802EBDA (epílogo -> retorna 0)
    //   0x4802EBF0  (alta do bne.w)->  nop
    // Modo 99 -> epílogo -> mt_boot segue para fastboot_init. O modo 200 cai
    // no próprio handler (idêntico); 101/exóticos passam pelo prelude do 200
    // e convergem ao mesmo destino de antes; 6-9/110/0-5 intocados.
    // O padrão tem 1 única ocorrência no binário (verificado).
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x2B6E, 0xD072, 0x2BC8, 0xF47F, 0xAF74);
    if (addr) {
        printf("Found boot mode dispatcher at 0x%08X, adding FASTBOOT(99) case\n", addr + 4);
        PATCH_MEM(addr + 4, 0x2B63, // cmp r3, #99
                  0xD0F4,           // beq 0x4802EBDA (epílogo do dispatcher)
                  0xBF00);          // nop
    } else {
        printf("WARN: boot mode dispatcher pattern not found!\n");
    }

    // Misc primeiro: 'adb reboot bootloader' deixa o comando "bootloader" na
    // misc; parse_bootloader_messages() já faz set_bootmode(99) + overlay.
    parse_bootloader_messages();

    // Vol-Up+power também precisa SETAR o modo 99 — antes só mostrava o
    // overlay e o dispatcher via mode 0 e bootava o Android normalmente.
    if (mtk_detect_key(VOLUME_UP)) {
        set_bootmode(BOOTMODE_FASTBOOT);
        show_bootmode(BOOTMODE_FASTBOOT);
    }

    // Família de 'reboot' do W7: quatro bl -> fastboot_register encadeados
    // (reboot / reboot-bootloader / reboot-recovery / reboot-fastboot) em
    // 0x48030D68..0x48030DA4 (0x14 de passo). NOPar os quatro e registrar o
    // nosso; 'continue' (0x48030D54) fica intacto.
    NOP(0x48030D68, 2); // reboot
    NOP(0x48030D7C, 2); // reboot-bootloader
    NOP(0x48030D90, 2); // reboot-recovery
    NOP(0x48030DA4, 2); // reboot-fastboot
    fastboot_register("reboot", cmd_reboot, 1);

    // Verified boot warning: 2B02 D052 2B03 D03A bate em 0x48027852 (igual ao
    // mcaymanlm). 2 = orange; NOPar o beq faz cair fora do aviso.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x2B02, 0xD052, 0x2B03, 0xD03A);
    if (addr) {
        printf("Found orange state warning at 0x%08X\n", addr + 2);
        NOP(addr + 2, 1);
    }
}
