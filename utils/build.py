#!/usr/bin/env python3

import argparse
import multiprocessing
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path
from liblk import LkImage

def get_cfg(path, key):
    with open(path) as f:
        for line in f:
            if line.startswith('CONFIG_' + key + '='):
                return line.strip().split('=', 1)[1]
    return None

class TermColors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[0;33m'
    BLUE = '\033[0;34m'
    PURPLE = '\033[0;35m'
    CYAN = '\033[0;36m'
    NC = '\033[0m'


class KaeruPatcher:
    ROOT_DIR = Path(__file__).resolve().parent.parent
    UTILS_DIR = ROOT_DIR / 'utils'
    CONFIGS_DIR = ROOT_DIR / 'configs'
    OUTPUT_SUFFIX = '-kaeru.bin'

    def __init__(self, device, lk_path):
        self.device = device
        self.config = f'{device}_defconfig'
        self.lk_path = Path(lk_path)
        self.debug_mode = os.environ.get('DEBUG') is not None

    def log(self, log_type, message):
        color = TermColors.BLUE
        prefix = 'INFO'

        if log_type == 'INFO':
            color = TermColors.BLUE
            prefix = 'INFO'
        elif log_type == 'SUCCESS':
            color = TermColors.GREEN
            prefix = 'SUCCESS'
        elif log_type == 'WARNING':
            color = TermColors.YELLOW
            prefix = 'WARNING'
        elif log_type == 'ERROR':
            color = TermColors.RED
            prefix = 'ERROR'
        elif log_type == 'DEBUG':
            color = TermColors.PURPLE
            prefix = 'DEBUG'
            if not self.debug_mode:
                return
        elif log_type == 'STEP':
            color = TermColors.CYAN
            prefix = 'STEP'

        output = f'{color}[{prefix}]{TermColors.NC} {message}'

        if log_type == 'ERROR':
            print(output, file=sys.stderr)
        else:
            print(output)

    def find_defconfig(self):
        config_filename = f'{self.device}_defconfig'

        config_path = self.CONFIGS_DIR / config_filename
        if config_path.is_file():
            self.log('DEBUG', f'Found config at: {config_path}')
            return config_path

        for config_path in self.CONFIGS_DIR.rglob(config_filename):
            if config_path.is_file():
                self.log('DEBUG', f'Found config at: {config_path}')
                return config_path

        return None

    def validate_inputs(self):
        self.log('DEBUG', f'Device: {self.device}')
        self.log('DEBUG', f'Config: {self.config}')
        self.log('DEBUG', f'Bootloader: {self.lk_path}')

        config_path = self.find_defconfig()
        if not config_path:
            self.log(
                'ERROR',
                f'Configuration file for {self.device} not found in {self.CONFIGS_DIR} or its subdirectories',
            )
            return False

        self.config_path = config_path

        if not self.lk_path.exists():
            self.log('ERROR', f'LK path {self.lk_path} does not exist')
            return False

        return True

    def clean_environment(self):
        self.log('STEP', 'Cleaning build environment')

        try:
            subprocess.run(
                ['make', 'clean'],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                check=True,
            )
            self.log('DEBUG', 'Make clean successful')
        except subprocess.CalledProcessError:
            self.log(
                'WARNING', 'Make clean returned non-zero, continuing anyway'
            )

        try:
            subprocess.run(
                ['make', 'distclean'],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                check=True,
            )
            self.log('DEBUG', 'Make distclean successful')
        except subprocess.CalledProcessError:
            self.log(
                'WARNING', 'Make distclean returned non-zero, continuing anyway'
            )

        self.log('SUCCESS', 'Environment cleaned')
        return True

    def build_project(self):
        self.log('STEP', f'Building configuration for {self.device}')

        try:
            subprocess.run(['make', self.config], check=True)
        except subprocess.CalledProcessError:
            self.log('ERROR', f'Failed to configure for {self.device}')
            return False

        cores = multiprocessing.cpu_count()
        self.log('DEBUG', f'Detected {cores} CPU cores')

        self.log('INFO', f'Building with {cores} cores')
        try:
            subprocess.run(['make', f'-j{cores}'], check=True)
        except subprocess.CalledProcessError:
            self.log('ERROR', 'Build failed')
            return False

        self.log('SUCCESS', 'Build completed successfully')
        return True

    def apply_patches(self):
        output_file = f'{self.device}{self.OUTPUT_SUFFIX}'

        self.log('STEP', f'Applying patches and generating {output_file}')

        lk_path = Path(self.lk_path)
        stage1_enabled = get_cfg(self.config_path, "STAGE1_SUPPORT") == 'y'

        if stage1_enabled:
            payload = 'stageone'
            temp_stage1 = Path(f'{self.device}-stage1.bin')
        else:
            payload = 'kaeru'
            temp_stage1 = None

        cmd_args = [
            str(self.UTILS_DIR / 'patch.py'),
            str(self.config_path),
            str(lk_path),
            payload,
            '-o',
            str(output_file if not stage1_enabled else temp_stage1),
        ]
        self.log('DEBUG', f'Running: python3 {" ".join(cmd_args)}')

        try:
            subprocess.run([sys.executable] + cmd_args, check=True)
        except subprocess.CalledProcessError:
            self.log('ERROR', 'Patching failed')
            return False

        if not stage1_enabled:
            output_path = Path(output_file)
            if output_path.is_file():
                self.log('SUCCESS', f'Patching completed: {output_file} created ({output_path.stat().st_size} bytes)')
                return True
            self.log('WARNING', 'Patching succeeded but no output file was created')
            return False

        lk = LkImage(str(temp_stage1))

        with open("kaeru", "rb") as f:
            kaeru_data = f.read()

        if 'kaeru' in lk.partitions:
            self.log('DEBUG', 'Partition kaeru already exists, removing it')
            lk.remove_partition('kaeru')

        lk.add_partition(
            name='kaeru',
            data=kaeru_data,
            memory_address=0,
            use_extended=True,
        )

        lk.save(output_file)
        self.log('SUCCESS', f'Kaeru partition injected and final image saved: {output_file}')

        self.log('DEBUG', f'Removing temporary stage 1 LK image: {temp_stage1}')
        temp_stage1.unlink()

        return True

    def run(self):
        self.log('INFO', f'Starting build process for device: {self.device}')

        if not self.validate_inputs():
            return 1

        try:
            if not self.clean_environment():
                return 1
            if not self.build_project():
                return 1
            if not self.apply_patches():
                return 1

            self.log('SUCCESS', 'Build process completed successfully')
            return 0
        except KeyboardInterrupt:
            self.log('ERROR', 'Process interrupted by user')
            return 1


def parse_arguments():
    parser = argparse.ArgumentParser(
        description='Kaeru Bootloader Patcher',
        epilog='Environment Variables: DEBUG - Set to any value to enable debug output',
    )
    parser.add_argument(
        'device_codename', help='Code name for the target device'
    )
    parser.add_argument('lk_path', help='Path to the LK directory/file')

    return parser.parse_args()


def main():
    args = parse_arguments()
    patcher = KaeruPatcher(args.device_codename, args.lk_path)
    return patcher.run()


if __name__ == '__main__':
    sys.exit(main())
