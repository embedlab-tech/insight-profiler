# Building Insight Profiler Firmware

The firmware uses [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/) and is built inside a [Nix](https://nixos.org/) development shell — no global toolchain installation required.

## Prerequisites

### 1. Install Nix

The recommended installer for macOS is [Determinate Systems Nix Installer](https://github.com/DeterminateSystemsLabs/nix-installer) — it handles macOS quirks automatically and is fully uninstallable.

```bash
curl --proto '=https' --tlsv1.2 -sSf -L https://install.determinate.systems/nix | sh -s -- install
```

Restart your terminal after installation.

> **Note:** The Determinate installer enables Nix Flakes by default. If you used the official Nix installer instead, add the following line to `~/.config/nix/nix.conf`:
> ```
> experimental-features = nix-flakes nix-command
> ```

### 2. Clone the repository

```bash
git clone https://github.com/embedlab-tech/insight-profiler.git
cd insight-profiler
```

## Enter the development shell

```bash
nix develop
```

The first run will download the ESP-IDF toolchain and all dependencies (~1.6 GB). This is a one-time operation — subsequent runs use the Nix cache and are instant.

What gets installed inside the shell:
- ESP-IDF v5.5 with the `xtensa-esp32s3-elf` cross-compiler
- `cmake`, `ninja`, `git`
- All Python dependencies required by ESP-IDF

> **Optional — direnv:** If you have [direnv](https://direnv.net/) installed, the shell activates automatically when you enter the project directory:
> ```bash
> direnv allow
> ```

## Build the firmware

```bash
cd firmware
idf.py build
```

A successful build produces the firmware binary at:
```
firmware/build/insight_profiler.bin
```

## Flash & monitor

Connect the ESP32-S3 board via USB-C, then:

```bash
# Flash firmware
idf.py -p /dev/cu.usbmodem* flash

# Open serial monitor (Ctrl+] to exit)
idf.py -p /dev/cu.usbmodem* monitor

# Flash and monitor in one step
idf.py -p /dev/cu.usbmodem* flash monitor
```

> On Linux the port will be `/dev/ttyACM0` or `/dev/ttyUSB0`.

## Clean build

```bash
cd firmware
rm -rf build
idf.py build
```

## Troubleshooting

**`nix develop` fails with evaluation error**
Make sure `flake.nix` is tracked by Git:
```bash
git add flake.nix
nix develop
```

**`idf.py: command not found`**
You are outside the Nix shell. Run `nix develop` first, or set up direnv.

**Port not found when flashing**
List available ports and use the correct one:
```bash
ls /dev/cu.usbmodem*   # macOS
ls /dev/ttyACM*        # Linux
```

**Build fails after pulling new changes**
Clean the build directory and rebuild:
```bash
cd firmware && rm -rf build && idf.py build
```
