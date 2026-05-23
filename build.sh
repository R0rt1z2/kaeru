#!/bin/bash
#
# SPDX-FileCopyrightText: 2026 Roger Ortiz <roger@r0rt1z2.com>
# SPDX-License-Identifier: AGPL-3.0-or-later
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() {
    echo "Usage: $0 [--debug] <codename> <bootloader>"
    exit 1
}

POSITIONAL=()
while [ $# -gt 0 ]; do
    case "$1" in
        --debug)
            KAERU_DEBUG=1
            shift
            ;;
        -h|--help)
            usage
            ;;
        --)
            shift
            POSITIONAL+=("$@")
            break
            ;;
        -*)
            echo "Unknown option: $1"
            usage
            ;;
        *)
            POSITIONAL+=("$1")
            shift
            ;;
    esac
done

if [ ${#POSITIONAL[@]} -ne 2 ]; then
    usage
fi

DEVICE="${POSITIONAL[0]}"
BOOTLOADER="${POSITIONAL[1]}"
OUTPUT="${DEVICE}-kaeru.bin"

UTILS_DIR="$SCRIPT_DIR/utils"
CONFIGS_DIR="$SCRIPT_DIR/configs"

CONFIG_PATH=$(find "$CONFIGS_DIR" -name "${DEVICE}_defconfig" -type f | head -n 1)

if [ -z "$CONFIG_PATH" ]; then
    echo "Config for $DEVICE not found"
    exit 1
fi

if [ ! -e "$BOOTLOADER" ]; then
    echo "$BOOTLOADER not found!"
    exit 1
fi

if [ "${KAERU_DEBUG:-0}" = "1" ]; then
    printf '\n\033[1;33m!! Debug mode enabled, verbose logging will be compiled in !!\033[0m\n\n'
    export KAERU_DEBUG=1
fi


make clean >/dev/null 2>&1 || true
make distclean >/dev/null 2>&1 || true

make "${DEVICE}_defconfig"
make -j"$(nproc)"

if grep -q "^CONFIG_STAGE1_SUPPORT=y" "$CONFIG_PATH"; then
    python3 "$UTILS_DIR/patch.py" "$CONFIG_PATH" "$BOOTLOADER" kaeru -l stageone -o "$OUTPUT"
else
    python3 "$UTILS_DIR/patch.py" "$CONFIG_PATH" "$BOOTLOADER" kaeru -o "$OUTPUT"
fi