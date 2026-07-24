#!/bin/sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

echo "Installing kaeru dependencies..."
sudo apt-get update -y
sudo apt-get install --fix-missing -y \
    gcc-arm-linux-gnueabihf \
    python3 \
    python3-pip \
    git \
    make \
    nano \
    android-tools-adb \
    android-tools-fastboot

echo "Installing kaeru Python dependencies..."
pip install --break-system-packages -r "${REPO_DIR}/utils/requirements.txt"

echo "Done, kaeru is ready to build!"