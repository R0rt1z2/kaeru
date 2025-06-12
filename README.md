# かえる 🐸
**Language:** [日本語](./README_ja-JP.md)

This repository contains an ARMv7 payload that provides arbitrary code execution on MediaTek bootloaders (LK).

## Overview
>[!CAUTION]
> If you don't know what you're doing, **you can brick your device**. This is not a beginner's project. Please **[READ THE DOCUMENTATION](https://github.com/R0rt1z2/kaeru/wiki)** carefully and understand the implications of modifying your bootloader.

Given a bootloader image, this tool will output a patched version that can be flashed to the device. The patched image will contain a custom payload that allows you to run arbitrary code during the boot process.

Things to keep in mind:
- This payload is designed for devices using an **ARMv7** Little Kernel (LK) as their bootloader.
- Offsets and addresses are specific to the device and bootloader version. You may need to adjust them for your specific device and bootloader version.

The following list showcases the most common use cases for `kaeru`:
- **Advanced debugging**: Use the payload to debug the boot process of your device by hooking into the bootloader's functions and variables.
- **Custom fastboot commands**: Add your own fastboot commands to the bootloader.
- **Remap button combinations**: You can set up custom boot modes and key combinations to enter different modes (e.g., recovery, fastboot, etc.).
- **Remove bootloader warnings**: Remove the "unlocked bootloader" warning that appears when the device is booted with an unlocked bootloader.

<small>__... and much more!__</small>

## Wiki

An elaborate wiki with multiple guides and notes has been provided to help you understand how kaeru works. Please refer to it to learn how to build, add support for a new device, and more:
1. [Table of contents](https://github.com/R0rt1z2/kaeru/wiki)
2. [Introduction](https://github.com/R0rt1z2/kaeru/wiki/Introduction)
3. [Can I use kaeru on my device?](https://github.com/R0rt1z2/kaeru/wiki/Can-I-use-kaeru-on-my-device%3F)
4. [Porting kaeru to a new device](https://github.com/R0rt1z2/kaeru/wiki/Porting-kaeru-to-a-new-device)
5. [Customization and kaeru APIs](https://github.com/R0rt1z2/kaeru/wiki/Customization-and-kaeru-APIs)

## License

This project is licensed under the **GNU Affero General Public License v3.0 (AGPL-3.0)**.

Key points to be aware of:

* You are free to use, modify, and distribute the software.
* If you modify and use the software publicly, you must release your source code.
* You must retain the same license (`AGPL-3.0`) when redistributing modified versions.
* You cannot keep modifications private if the software is used to provide a networked service.

For full details, please refer to the [LICENSE](https://github.com/R0rt1z2/kaeru/tree/master/LICENSE) file.

### Copyright & Developers

- **© 2023–2025 [Skidbladnir Labs, S.L.](https://skidbladnir.cat/)**
- Developed by:
    - Roger Ortiz ([`@R0rt1z2`](https://github.com/R0rt1z2)) ([me@r0rt1z2.com](mailto:me@r0rt1z2.com))
    - Mateo De la Hoz ([`@AntiEngineer`](https://github.com/AntiEngineer)) ([me@antiengineer.com](mailto:me@antiengineer.com))

## Acknowledgments

### Linux
Some of the Makefile and build-related scripts were adapted from the [`Linux` kernel](https://github.com/torvalds/linux) source code.

`Linux` which is licensed under the [GNU General Public License v2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html).

> © The Linux Foundation and contributors.  

> <small>_Original source: https://github.com/torvalds/linux_</small>

### nanoprintf
This project includes [`nanoprintf`](https://github.com/charlesnicholson/nanoprintf), a minimal implementation of `printf`-style formatting designed for embedded systems.

`nanoprintf` is dual-licensed under the [Unlicense](http://unlicense.org) and the [Zero-Clause BSD (0BSD)](https://opensource.org/licenses/0BSD). 

> © 2019 Charles Nicholson.

> <small>_Original source: https://github.com/charlesnicholson/nanoprintf_</small>

## Disclaimer

This software is provided **"as is"** without any warranty of any kind, express or implied. By using this tool, you acknowledge that:

* Modifying the bootloader or flashing modified images carries **a high risk of permanently damaging your device** (bricking).
* You are solely responsible for any consequences resulting from the use, misuse, or inability to use this software.
* The maintainers and contributors of this project are **not liable for any damage**, data loss, device malfunction, or legal issues that may arise.
* This project is intended for **educational and research purposes only**. It is **not intended for illegal or unauthorized use**.

Proceed only if you fully understand the risks and implications.
