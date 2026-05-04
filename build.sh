#!/bin/bash
#
# SPDX-FileCopyrightText: 2026 Roger Ortiz <me@r0rt1z2.com>
# SPDX-License-Identifier: AGPL-3.0-or-later
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ $# -ne 2 ]; then
    echo "Usage: $0 <codename> <bootloader>"
    exit 1
fi

DEVICE="$1"
BOOTLOADER="$2"

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

make clean >/dev/null 2>&1 || true
make distclean >/dev/null 2>&1 || true

make "${DEVICE}_defconfig"
make -j"$(nproc)"

if grep -q "^CONFIG_STAGE1_SUPPORT=y" "$CONFIG_PATH"; then
    python3 "$UTILS_DIR/patch.py" "$CONFIG_PATH" "$BOOTLOADER" kaeru -l stageone
else
    python3 "$UTILS_DIR/patch.py" "$CONFIG_PATH" "$BOOTLOADER" kaeru
fi