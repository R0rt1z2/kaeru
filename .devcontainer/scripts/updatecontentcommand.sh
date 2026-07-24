#!/bin/sh

set -e

echo "Installing Kaeru dependencies..."
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

# i assume this script will run in repository root
echo "Installing Kaeru Python dependencies"
pip install --break-system-packages -r "$PWD/utils/requirements.txt"

echo "oncreatecommand.sh done!"

exit 0
