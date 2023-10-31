# kaeru

![License](https://img.shields.io/github/license/R0rt1z2/kaeru?color=blue)
![GitHub Issues](https://img.shields.io/github/issues-raw/R0rt1z2/kaeru?color=red)

`kaeru` is a payload that functions as an ARMv7 bootstrapper for MediaTek bootloaders, known as _Little Kernel_ (LK). Allows arbitrary code execution with full permissions, doing so after the hardware initialization is complete but before the execution of the main LK function, known as _app_.
> <small>[!WARNING]
> _I'm **NOT** responsible for any data loss, or the result of a device being hard-bricked. The user of this payload accepts responsibility should something happen to their device._</small>

### How does it work?
The code of the payload is similar to [amonet](https://github.com/chaosmaster/amonet) or [kamakiri](https://github.com/amonet-kamakiri/kamakiri). It's built as THUMB code and injected into the LK image right after the stack is set up, which helps avoid any overwrites or misfired executions. This is done by the [`inject_payload.py`](https://github.com/R0rt1z2/kaeru/blob/mt8163-aquaris_m8/tools/inject_payload.py) script, which contains all the injection logic. Once it's in place, it also modifies the LK's flow, causing it to jump to our payload instead of executing the original main function. It offers a range of practical applications. From disabling 'unlock warnings' to implementing custom fastboot commands. After performing the specified operations, the payload jumps back to the original LK code, allowing the device to proceed with its booting sequence as intended.
> <small>[!WARNING]
> _This payload is **ONLY** meant to be used in MediaTek devices with no SBC (Secure Boot Control) protection._</small>

### Requirements
In order to build and inject the payload, you'll need the following:
* A copy of the LK image you want to inject the payload into.
* The compiler, which can be installed with `sudo apt install gcc-arm-none-eabi` on Debian-based distros.
* Python 3, which can be installed with `sudo apt install python3` on Debian-based distros.
* In addition, my `liblk` library is also required, which can be found [here](https://github.com/R0rt1z2/liblk).

### Building
To build the payload, you just need to run `make` in the `payload` folder. It should generate the payload binary as `build/payload.bin`.
> <small>[!NOTE]
> _If porting the payload to a new device, you'll need to modify all the offsets found in the `payload/common.h` file._</small>

### Injecting
To inject the payload, you'll need to run the `inject_payload.py` script, which can be found in the `tools` folder and takes the following arguments:

* `inject_payload.py [-h] [-o OUTPUT] lk payload payload_address caller_address`
> <small>[!NOTE]
> _If porting the payload to a new device, you'll need to modify the last two arguments of the script._</small>

## License
This project is licensed under the GPLv3 license - see the [`LICENSE`](https://github.com/R0rt1z2/kaeru/blob/mt8163-aquaris_m8/LICENSE) file for details.