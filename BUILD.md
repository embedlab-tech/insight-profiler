# Building Insight Profiler

The project uses [Nix](https://nixos.org/) to provide reproducible development environments for all three components — firmware, Python SDK, and JS/TypeScript SDK. No global toolchain installation is required.

## 1. Install Nix

Use the [Determinate Systems installer](https://github.com/DeterminateSystemsLabs/nix-installer) — it handles macOS quirks automatically, enables Flakes by default, and is fully uninstallable.

```bash
curl --proto '=https' --tlsv1.2 -sSf -L https://install.determinate.systems/nix | sh -s -- install
```

Restart your terminal after installation.

> **Note:** If you used the official Nix installer instead of Determinate Systems, manually enable Flakes:
> ```bash
> echo "experimental-features = nix-flakes nix-command" >> ~/.config/nix/nix.conf
> ```

## 2. Clone the repository

```bash
git clone https://github.com/embedlab-tech/insight-profiler.git
cd insight-profiler
```

## 3. Enter a development shell

The project provides focused shells per component, and one combined default shell.

### All-in-one (firmware + Python SDK + JS SDK)

```bash
nix develop
```

### Focused shells

```bash
nix develop .#firmware     # ESP-IDF v5.x + xtensa toolchain
nix develop .#python-sdk   # Python 3.12 + uv + ruff
nix develop .#js-sdk       # Node.js 22 + npm
```

> **direnv (optional):** Activates the default shell automatically when you enter the project directory:
> ```bash
> direnv allow
> ```

**First run:** downloads the ESP-IDF toolchain and all dependencies (~1.6 GB). This is a one-time operation — subsequent runs use the Nix cache and are instant.

---

## Firmware (ESP32-S3)

```bash
nix develop .#firmware
cd firmware

# Build
idf.py build

# Flash + monitor (connect ESP32-S3 via USB-C first)
idf.py -p /dev/cu.usbmodem* flash monitor
```

The compiled binary is at `firmware/build/insight_profiler.bin`.

> On Linux use `/dev/ttyACM0` or `/dev/ttyUSB0` instead of `/dev/cu.usbmodem*`.

**Clean build:**
```bash
rm -rf firmware/build && idf.py build
```

---

## Python SDK

```bash
nix develop .#python-sdk
cd sdk-python

# Install dependencies into a local virtualenv
uv sync

# Run tests
uv run pytest

# Lint + format
uv run ruff check .
uv run ruff format .

# Type check
uv run mypy .
```

---

## JS / TypeScript SDK

```bash
nix develop .#js-sdk
cd sdk-js

# Install dependencies
npm install

# Build
npm run build

# Run tests
npm test

# Lint
npm run lint
```

---

## Troubleshooting

**`nix develop` fails with "path is not tracked by Git"**
```bash
git add flake.nix
nix develop
```

**`idf.py: command not found`**
You are outside the Nix shell. Run `nix develop .#firmware` first, or set up direnv.

**Port not found when flashing**
```bash
ls /dev/cu.usbmodem*   # macOS
ls /dev/ttyACM*        # Linux
```

**Build fails after pulling changes**
```bash
rm -rf firmware/build && idf.py build
```
