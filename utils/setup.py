#!/usr/bin/env python3

import argparse
import datetime
import os
import re
import subprocess
import sys
from pathlib import Path


class TermColors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[0;33m'
    BLUE = '\033[0;34m'
    PURPLE = '\033[0;35m'
    CYAN = '\033[0;36m'
    NC = '\033[0m'


class DeviceSetup:
    ROOT_DIR = Path(__file__).resolve().parent.parent
    UTILS_DIR = ROOT_DIR / 'utils'
    CONFIGS_DIR = ROOT_DIR / 'configs'
    BOARDS_DIR = ROOT_DIR / 'board'
    SOCS_DIR = ROOT_DIR / 'soc'

    def __init__(self, debug=False):
        self.debug = debug

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
            if not self.debug:
                return
        elif log_type == 'STEP':
            color = TermColors.CYAN
            prefix = 'STEP'

        output = f'{color}[{prefix}]{TermColors.NC} {message}'

        if log_type == 'ERROR':
            print(output, file=sys.stderr)
        else:
            print(output)

    def get_git_info(self):
        name = 'Your name'
        email = 'your.email@example.com'

        try:
            import configparser

            git_configs = [
                Path.home() / '.gitconfig',
                Path('.git/config') if (Path('.git')).exists() else None,
                Path('/etc/gitconfig') if os.name != 'nt' else None,
            ]

            config = configparser.ConfigParser()
            for git_config in git_configs:
                if git_config and git_config.exists():
                    try:
                        config.read(git_config)
                        if 'user' in config and 'name' in config['user']:
                            name = config['user']['name']
                            self.log('DEBUG', f'Got git user.name: {name}')
                        if 'user' in config and 'email' in config['user']:
                            email = config['user']['email']
                            self.log('DEBUG', f'Got git user.email: {email}')

                        if (
                            name != 'Your name'
                            and email != 'your.email@example.com'
                        ):
                            break
                    except Exception as e:
                        self.log(
                            'DEBUG',
                            f'Error reading git config {git_config}: {e}',
                        )
                        continue

            if name == 'Your name':
                try:
                    result = subprocess.run(
                        ['git', 'config', '--get', 'user.name'],
                        capture_output=True,
                        text=True,
                        check=False,
                    )
                    if result.returncode == 0 and result.stdout.strip():
                        name = result.stdout.strip()
                        self.log(
                            'DEBUG',
                            f'Got git user.name from subprocess: {name}',
                        )
                except (subprocess.SubprocessError, FileNotFoundError):
                    self.log('DEBUG', 'Using default name: Your name')

            if email == 'your.email@example.com':
                try:
                    result = subprocess.run(
                        ['git', 'config', '--get', 'user.email'],
                        capture_output=True,
                        text=True,
                        check=False,
                    )
                    if result.returncode == 0 and result.stdout.strip():
                        email = result.stdout.strip()
                        self.log(
                            'DEBUG',
                            f'Got git user.email from subprocess: {email}',
                        )
                except (subprocess.SubprocessError, FileNotFoundError):
                    self.log(
                        'DEBUG', 'Using default email: your.email@example.com'
                    )

        except ImportError:
            self.log(
                'DEBUG', 'configparser not available, falling back to defaults'
            )

        return f'{name} <{email}>'

    def to_upper(self, text):
        return text.upper()

    def to_lower(self, text):
        return text.lower()

    def sanitize_vendor_path(self, vendor):
        return self.to_lower(vendor).replace(' ', '_')

    def sanitize_config_name(self, name):
        return self.to_upper(name).replace(' ', '_').replace('-', '_')

    def get_user_input(self, prompt, default=None):
        if default:
            value = input(f'{prompt} [{default}]: ')
            return value if value else default
        else:
            return input(f'{prompt}: ')

    def find_existing_defconfig(self, codename):
        config_filename = f'{codename}_defconfig'
        
        for config_path in self.CONFIGS_DIR.rglob(config_filename):
            if config_path.is_file():
                return config_path
        
        return None

    def check_existing_device(self, codename, vendor):
        kconfig_path = self.BOARDS_DIR / 'Kconfig'
        makefile_path = self.BOARDS_DIR / 'Makefile'
        
        if kconfig_path.exists():
            kconfig_content = kconfig_path.read_text()
            if f'CONFIG_{vendor}_{codename}' in kconfig_content:
                self.log(
                    'ERROR',
                    f'Device {vendor}_{codename} already exists in board/Kconfig',
                )
                return False

        if makefile_path.exists():
            makefile_content = makefile_path.read_text()
            if f'CONFIG_{vendor}_{codename}' in makefile_content:
                self.log(
                    'ERROR',
                    f'Device {vendor}_{codename} already exists in board/Makefile',
                )
                return False

        existing_defconfig = self.find_existing_defconfig(codename)
        if existing_defconfig:
            self.log(
                'ERROR', 
                f'Config file {codename}_defconfig already exists at {existing_defconfig}'
            )
            return False

        return True

    def check_soc_exists(self, soc):
        kconfig_path = self.SOCS_DIR / 'Kconfig'

        if not soc.startswith('MT'):
            soc = f'MT{soc}'

        if kconfig_path.exists():
            kconfig_content = kconfig_path.read_text()
            if f'MEDIATEK_{soc}' in kconfig_content:
                return True

        return False

    def add_device_to_kconfig(self, vendor, codename, model):
        self.log('STEP', 'Adding device entry to board/Kconfig')

        kconfig_path = self.BOARDS_DIR / 'Kconfig'
        if not kconfig_path.exists():
            self.log('ERROR', 'Kconfig file not found')
            return False

        kconfig_content = kconfig_path.read_text()

        match = re.search(r'^endmenu', kconfig_content, re.MULTILINE)
        if not match:
            self.log(
                'ERROR',
                'Could not find end of Device Support section in Kconfig',
            )
            return False

        device_section_end = match.start()
        self.log(
            'DEBUG',
            f'Found Device Support section endmenu at position: {device_section_end}',
        )

        vendor_upper = self.to_upper(vendor)
        codename_upper = self.to_upper(codename)

        new_entry = f"""
    config {vendor_upper}_{codename_upper}
        bool "Support {model}"
        default n
        help
          Say Y if you want to include support for {model}
"""

        new_content = (
            kconfig_content[:device_section_end]
            + new_entry
            + kconfig_content[device_section_end:]
        )

        kconfig_path.write_text(new_content)

        self.log('SUCCESS', 'Device entry added to board/Kconfig')
        return True

    def add_device_to_makefile(self, vendor, codename):
        self.log('STEP', 'Adding device entry to board/Makefile')

        makefile_path = self.BOARDS_DIR / 'Makefile'

        vendor_path = self.sanitize_vendor_path(vendor)
        vendor_upper = self.to_upper(vendor)
        codename_upper = self.to_upper(codename)

        if makefile_path.exists() and makefile_path.stat().st_size > 0:
            with open(makefile_path, 'r+') as f:
                content = f.read()
                if not content.endswith('\n'):
                    f.write('\n')
                    self.log(
                        'DEBUG', 'Added missing newline at the end of Makefile'
                    )

        with open(makefile_path, 'a') as f:
            f.write(
                f'lib-$(CONFIG_{vendor_upper}_{codename_upper}) += {vendor_path}/board-{codename}.o\n'
            )

        self.log('SUCCESS', 'Device entry added to board/Makefile')
        return True

    def create_board_file(self, vendor, codename, model, copyright):
        self.log('STEP', f'Creating board file for {vendor}/{codename}')

        vendor_path = self.sanitize_vendor_path(vendor)
        year = datetime.datetime.now().year

        vendor_dir = self.BOARDS_DIR / vendor_path
        vendor_dir.mkdir(parents=True, exist_ok=True)
        self.log('DEBUG', f'Created vendor directory: {vendor_dir}')

        board_file = vendor_dir / f'board-{codename}.c'

        board_content = f"""//
// SPDX-FileCopyrightText: {year} {copyright}
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

void board_early_init(void) {{
    printf("Entering early init for {model}\\n");
}}

void board_late_init(void) {{
    printf("Entering late init for {model}\\n");
}}
"""

        board_file.write_text(board_content)

        self.log('SUCCESS', f'Board file created: {board_file}')
        return True

    def create_defconfig(self, codename, vendor, lk_path, soc, vendor_subdir=False):
        self.log('STEP', f'Creating defconfig file for {codename}')

        if vendor_subdir:
            vendor_path = self.sanitize_vendor_path(vendor)
            vendor_config_dir = self.CONFIGS_DIR / vendor_path
            vendor_config_dir.mkdir(parents=True, exist_ok=True)
            defconfig_path = vendor_config_dir / f'{codename}_defconfig'
        else:
            defconfig_path = self.CONFIGS_DIR / f'{codename}_defconfig'

        self.log(
            'DEBUG',
            f'Running: python3 {self.UTILS_DIR}/parse.py {lk_path} {defconfig_path}',
        )

        try:
            subprocess.run(
                [
                    sys.executable,
                    str(self.UTILS_DIR / 'parse.py'),
                    str(lk_path),
                    str(defconfig_path),
                ],
                check=True,
            )
        except subprocess.CalledProcessError:
            self.log('ERROR', 'Failed to generate defconfig using parse.py')
            return False

        vendor_upper = self.to_upper(vendor)
        codename_upper = self.to_upper(codename)

        with open(defconfig_path, 'a') as f:
            f.write(f'CONFIG_{vendor_upper}_{codename_upper}=y\n')
            f.write(f'CONFIG_MEDIATEK_{soc}=y\n')

        self.log('SUCCESS', f'Defconfig file created: {defconfig_path}')
        return True

    def validate_inputs(self, codename, model, vendor, lk_path, soc):
        errors = 0

        if not codename:
            self.log('ERROR', 'Device codename is required')
            errors += 1

        if not model:
            self.log('ERROR', 'Device model is required')
            errors += 1

        if not vendor:
            self.log('ERROR', 'Device vendor is required')
            errors += 1

        if not lk_path:
            self.log('ERROR', 'Path to LK image is required')
            errors += 1
        elif not Path(lk_path).is_file():
            self.log('ERROR', f'LK image not found at {lk_path}')
            errors += 1

        if not soc:
            self.log('ERROR', 'MediaTek SoC model is required')
            errors += 1

        return errors == 0

    def interactive_mode(self):
        self.log('INFO', 'Running in interactive mode')

        codename = self.get_user_input('Enter device codename (e.g., penangf)')
        model = self.get_user_input('Enter device model (e.g., Motorola G13)')
        vendor = self.get_user_input(
            'Enter device vendor/brand (e.g., motorola)'
        )
        lk_path = self.get_user_input('Enter path to LK image file')
        soc = self.get_user_input('Enter MediaTek SoC model (e.g., MT6765)')

        if soc.startswith('MT'):
            soc = soc[2:]

        return codename, model, vendor, lk_path, soc

    def run(
        self, codename=None, model=None, vendor=None, lk_path=None, soc=None
    ):
        if not codename or not model or not vendor or not lk_path or not soc:
            codename, model, vendor, lk_path, soc = self.interactive_mode()

        if not self.validate_inputs(codename, model, vendor, lk_path, soc):
            self.log('ERROR', 'Input validation failed, exiting')
            return 1

        if soc.startswith('MT'):
            soc_internal = soc
        else:
            soc_internal = f'MT{soc}'

        copyright = self.get_git_info()

        self.log('INFO', 'Setting up device with the following parameters:')
        self.log('INFO', f'  Codename: {codename}')
        self.log('INFO', f'  Model: {model}')
        self.log('INFO', f'  Vendor: {vendor}')
        self.log('INFO', f'  LK Path: {lk_path}')
        self.log('INFO', f'  SoC: {soc_internal}')
        self.log('INFO', f'  Copyright: {copyright}')

        vendor_upper = self.to_upper(vendor)
        codename_upper = self.to_upper(codename)

        if not self.check_existing_device(codename_upper, vendor_upper):
            self.log('ERROR', 'Device already exists, exiting')
            return 1

        if not self.check_soc_exists(soc):
            self.log('ERROR', f'SoC {soc_internal} not found in soc/Kconfig')
            self.log(
                'ERROR',
                f'Please add {soc_internal} to the soc/Kconfig file manually first',
            )
            return 1

        if not self.add_device_to_kconfig(vendor, codename_upper, model):
            self.log('ERROR', 'Failed to add device entry to Kconfig, exiting')
            return 1

        if not self.add_device_to_makefile(vendor, codename):
            self.log('ERROR', 'Failed to add device entry to Makefile, exiting')
            return 1

        if not self.create_board_file(vendor, codename, model, copyright):
            self.log('ERROR', 'Failed to create board file, exiting')
            return 1

        if not self.create_defconfig(codename, vendor, lk_path, soc_internal, vendor_subdir=True):
            self.log('ERROR', 'Failed to create defconfig, exiting')
            return 1

        self.log('SUCCESS', 'Device setup completed successfully')
        self.log(
            'INFO', f'You can now build using: ./build.sh {codename} <lk_path>'
        )

        return 0


def parse_arguments():
    parser = argparse.ArgumentParser(
        description='Kaeru Device Setup',
        epilog='If options are omitted, the script will run in interactive mode.',
    )
    parser.add_argument(
        '-c', '--codename', help='Device codename (e.g., penangf)'
    )
    parser.add_argument(
        '-m', '--model', help='Device model (e.g., Motorola G13)'
    )
    parser.add_argument(
        '-v', '--vendor', help='Device vendor/brand (e.g., motorola)'
    )
    parser.add_argument('-l', '--lk', help='Path to the LK image file')
    parser.add_argument('-s', '--soc', help='MediaTek SoC model (e.g., MT6765)')
    parser.add_argument(
        '-d', '--debug', action='store_true', help='Enable debug output'
    )

    return parser.parse_args()


def main():
    args = parse_arguments()

    setup = DeviceSetup(debug=args.debug)

    try:
        return setup.run(
            codename=args.codename,
            model=args.model,
            vendor=args.vendor,
            lk_path=args.lk,
            soc=args.soc,
        )
    except KeyboardInterrupt:
        setup.log('ERROR', 'Process interrupted by user')
        return 1


if __name__ == '__main__':
    sys.exit(main())
