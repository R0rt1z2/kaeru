//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <lib/common.h>
#include <lib/debug.h>
#include <lib/fastboot.h>
#include <lib/idmelib.h>
#include <lib/mt_part.h>
#include <lib/string.h>

#define IDME_MAX_ARGS 4
#define IDME_ARG_LEN  33

static bool idme_access(void *buf, bool write) {
    struct device_t *dev = mt_part_get_device();

    if (!dev || dev->init != 1) {
        return false;
    }

    if (write) {
        return dev->write(dev, buf, 0, IDME_SIZE, CONFIG_IDME_EMMC_PART) ==
               IDME_SIZE;
    }

    return dev->read(dev, 0, buf, IDME_SIZE, CONFIG_IDME_EMMC_PART) ==
           IDME_SIZE;
}

static int idme_split(const char *arg, char argv[IDME_MAX_ARGS][IDME_ARG_LEN]) {
    int argc = 0;

    while (*arg && argc < IDME_MAX_ARGS) {
        while (*arg == ' ') {
            arg++;
        }

        if (!*arg) {
            break;
        }

        int i = 0;
        while (*arg && *arg != ' ') {
            if (i < IDME_ARG_LEN - 1) {
                argv[argc][i++] = *arg;
            }
            arg++;
        }

        argv[argc][i] = '\0';
        argc++;
    }

    return argc;
}

static void idme_value_str(const struct idme_item *item, char *buf,
                           size_t len) {
    if (!idmelib_item_has_data(item)) {
        npf_snprintf(buf, len, "(empty)");
        return;
    }

    if (strncmp(item->desc.name, "fos_flags", IDME_MAX_NAME_LEN) == 0 ||
        strncmp(item->desc.name, "dev_flags", IDME_MAX_NAME_LEN) == 0 ||
        strncmp(item->desc.name, "usr_flags", IDME_MAX_NAME_LEN) == 0) {
        char decoded[IDME_FLAGS_STR_LEN];
        unsigned long long v = parse_hex64((const char *)item->data, NULL);

        if (idmelib_flags_to_str(v, decoded, sizeof(decoded)) == 0) {
            npf_snprintf(buf, len, "%s", decoded);
            return;
        }
    }

    if (strncmp(item->desc.name, "bootmode", IDME_MAX_NAME_LEN) == 0) {
        int mode = (int)strtol((const char *)item->data, NULL, 10);

        npf_snprintf(buf, len, "%d (%s)", mode, idmelib_bootmode_to_str(mode));
        return;
    }

    for (uint32_t i = 0; i < item->desc.size && item->data[i]; i++) {
        if (item->data[i] < 0x20 || item->data[i] >= 0x7F) {
            size_t used = 0;

            for (uint32_t j = 0; j < item->desc.size && used + 3 < len; j++) {
                used += npf_snprintf(buf + used, len - used, "%02x",
                                     item->data[j]);
            }

            return;
        }
    }

    npf_snprintf(buf, len, "%.*s", (int)item->desc.size,
                 (const char *)item->data);
}

// The fastboot client prints INFO lines but drops the payload of the OKAY
// that ends an 'oem' command, so anything worth reading goes out as INFO.
// Long values get split rather than truncated at the reply size.
static void idme_reply(const char *value) {
    char line[FASTBOOT_INFO_MAX + 1];
    size_t len = strlen(value);

    for (size_t off = 0; off < len; off += FASTBOOT_INFO_MAX) {
        size_t n = len - off;

        if (n > FASTBOOT_INFO_MAX) {
            n = FASTBOOT_INFO_MAX;
        }

        // Byte at a time on purpose. value + off is not word aligned once
        // this wraps, and the optimised memcpy would fault on it.
        for (size_t i = 0; i < n; i++) {
            line[i] = value[off + i];
        }

        line[n] = '\0';
        fastboot_info(line);
    }

    fastboot_okay("");
}

static void idme_list(struct idme *hdr) {
    struct idme_item *item = idmelib_first_item(hdr);
    char value[IDME_FLAGS_STR_LEN];
    char line[FASTBOOT_INFO_MAX + 1];

    npf_snprintf(line, sizeof(line), "IDME v%.4s, %u items", hdr->version,
                 hdr->items_num);
    fastboot_info(line);

    for (uint32_t i = 0; i < hdr->items_num; i++) {
        idme_value_str(item, value, sizeof(value));
        npf_snprintf(line, sizeof(line), "%-16s %4u %s", item->desc.name,
                     item->desc.size, value);
        fastboot_info(line);

        item = idmelib_item_next(item);
    }
}

static int idme_flag_type(const char *str, enum idme_flag_type *type) {
    if (strcmp(str, "fos") == 0) {
        *type = IDME_FLAGS_FOS;
    } else if (strcmp(str, "dev") == 0) {
        *type = IDME_FLAGS_DEV;
    } else if (strcmp(str, "usr") == 0) {
        *type = IDME_FLAGS_USR;
    } else {
        return -1;
    }

    return 0;
}

static void idme_show_flags(enum idme_flag_type type, unsigned long long val) {
    char sym[IDME_FLAGS_STR_LEN];
    char line[FASTBOOT_INFO_MAX + 1];
    char hex[19];

    if (idmelib_flags_to_str(val, sym, sizeof(sym))) {
        sym[0] = '\0';
    }

    hex64(hex, val);
    npf_snprintf(line, sizeof(line), "%s = %s", idmelib_flags_var_name(type),
                 hex);
    fastboot_info(line);
    npf_snprintf(line, sizeof(line), "  %s", sym);
    fastboot_info(line);
}

static void idme_cmd_flags(struct idme *hdr, int argc,
                           char argv[IDME_MAX_ARGS][IDME_ARG_LEN]) {
    enum idme_flag_type type = IDME_FLAGS_FOS;
    unsigned long long cur = 0;
    unsigned long long val;
    unsigned long long next;
    int off = 1;

    if (argc > 1 && idme_flag_type(argv[1], &type) == 0) {
        off = 2;
    }

    if (argc <= off) {
        if (idmelib_flags_get(hdr, type, &cur)) {
            fastboot_fail("Cannot read that flags variable");
            return;
        }

        idme_show_flags(type, cur);
        fastboot_okay("");

        return;
    }

    if (argc <= off + 1) {
        fastboot_fail("Usage: oem idme flags [fos|dev|usr] set|add|remove <v>");
        return;
    }

    const char *end;
    val = parse_hex64(argv[off + 1], &end);
    if (*end && idmelib_flags_parse_name(argv[off + 1], &val)) {
        fastboot_fail("Value is neither a number nor a flag name");
        return;
    }

    if (idmelib_flags_get(hdr, type, &cur)) {
        cur = 0;
    }

    if (strcmp(argv[off], "set") == 0) {
        next = val;
    } else if (strcmp(argv[off], "add") == 0) {
        next = cur | val;
    } else if (strcmp(argv[off], "remove") == 0) {
        next = cur & ~val;
    } else {
        fastboot_fail("Expected set, add or remove");
        return;
    }

    if (idmelib_flags_set(hdr, type, next) || !idme_access(hdr, true)) {
        fastboot_fail("Could not write the IDME block back");
        return;
    }

    idme_show_flags(type, next);
    fastboot_okay("");
}

void cmd_idme(const char *arg, void *data, unsigned sz) {
    char argv[IDME_MAX_ARGS][IDME_ARG_LEN];
    char line[FASTBOOT_INFO_MAX + 1];

    int argc = idme_split(arg, argv);
    if (!argc) {
        fastboot_fail("Usage: oem idme list|read|write|flags|board|bootmode|"
                      "bootcount|version");
        return;
    }

    if (!data) {
        fastboot_fail("No download buffer to stage through");
        return;
    }

    struct idme *hdr = data;
    if (!idme_access(hdr, false)) {
        fastboot_fail("Could not read the IDME block");
        return;
    }

    if (!idmelib_magic_valid(hdr)) {
        fastboot_fail("IDME magic is wrong, is it on another boot area?");
        return;
    }

    if (strcmp(argv[0], "list") == 0) {
        idme_list(hdr);
        fastboot_okay("");
        return;
    }

    if (strcmp(argv[0], "flags") == 0) {
        idme_cmd_flags(hdr, argc, argv);
        return;
    }

    if (strcmp(argv[0], "read") == 0 && argc == 2) {
        const struct idme_item *item = idmelib_get_item(hdr, argv[1]);
        char value[IDME_FLAGS_STR_LEN];

        if (!item) {
            fastboot_fail("No such IDME entry");
            return;
        }

        idme_value_str(item, value, sizeof(value));
        idme_reply(value);
        return;
    }

    if (strcmp(argv[0], "write") == 0 && argc == 3) {
        const struct idme_item *item = idmelib_get_item(hdr, argv[1]);

        if (!item) {
            fastboot_fail("No such IDME entry");
            return;
        }

        if (strlen(argv[2]) > item->desc.size) {
            fastboot_fail("Value is longer than the entry, they cannot grow");
            return;
        }

        if (idmelib_set_var(hdr, argv[1], argv[2])) {
            fastboot_fail("Could not stage the new value");
            return;
        }

        if (!idme_access(hdr, true)) {
            fastboot_fail("Could not write the IDME block back");
            return;
        }

        fastboot_okay("");
        return;
    }

    if (strcmp(argv[0], "board") == 0) {
        struct idme_board_info info;

        if (idmelib_get_board_info(hdr, &info)) {
            fastboot_fail("Cannot read board_id");
            return;
        }

        npf_snprintf(line, sizeof(line), "board_id   %s", info.raw);
        fastboot_info(line);
        npf_snprintf(line, sizeof(line), "board_type 0x%04x", info.board_type);
        fastboot_info(line);
        npf_snprintf(line, sizeof(line), "board_rev  0x%02x", info.board_rev);
        fastboot_info(line);
        npf_snprintf(line, sizeof(line), "wan        %s",
                     info.has_wan ? "yes" : "no");
        fastboot_info(line);
        fastboot_okay("");
        return;
    }

    if (strcmp(argv[0], "bootmode") == 0) {
        int mode = idmelib_get_bootmode(hdr);

        if (mode < 0) {
            fastboot_fail("Cannot read bootmode");
            return;
        }

        npf_snprintf(line, sizeof(line), "%d (%s)", mode,
                     idmelib_bootmode_to_str(mode));
        idme_reply(line);
        return;
    }

    if (strcmp(argv[0], "bootcount") == 0) {
        if (argc == 2 && strcmp(argv[1], "reset") == 0) {
            if (idmelib_set_var(hdr, "bootcount", "0") ||
                !idme_access(hdr, true)) {
                fastboot_fail("Could not write the IDME block back");
                return;
            }

            idme_reply("0");
            return;
        }

        unsigned int count;
        if (idmelib_get_bootcount(hdr, &count)) {
            fastboot_fail("Cannot read bootcount");
            return;
        }

        npf_snprintf(line, sizeof(line), "%u", count);
        idme_reply(line);
        return;
    }

    if (strcmp(argv[0], "version") == 0) {
        if (argc == 2) {
            if (idmelib_set_version(hdr, argv[1]) || !idme_access(hdr, true)) {
                fastboot_fail("Could not set the version");
                return;
            }

            idme_reply(argv[1]);
            return;
        }

        char ver[IDME_VERSION_LEN + 1];
        if (idmelib_get_version(hdr, ver, sizeof(ver))) {
            fastboot_fail("Cannot read the version");
            return;
        }

        idme_reply(ver);
        return;
    }

    fastboot_fail("Unknown subcommand");
}

FASTBOOT_CMD(idme, "oem idme", cmd_idme, 1);
