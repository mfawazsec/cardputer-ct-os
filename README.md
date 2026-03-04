# M5-ctOS

A Watch Dogs-themed Bluetooth toolkit firmware for the **M5Stack Cardputer**. Fork of [m5stick-nemo](https://github.com/n0xa/m5stick-nemo) by n0xa, stripped down to a single focused tool: **BT Maelstrom**.

> For authorized security research and educational use only.

---

## Hardware

| | |
|---|---|
| **Device** | M5Stack Cardputer (ESP32-S3) |
| **Display** | 240×135 ST7789 LCD |
| **Input** | Built-in keyboard |
| **Wireless** | BLE 5.0 |

---

## What it does

BT Maelstrom cycles through all supported Bluetooth advertisement types simultaneously:

- **SourApple** — randomized Apple proximity pairing popups
- **Swift Pair** — Microsoft Swift Pair device discovery notifications
- **Android Fast Pair** — Google Fast Pair pairing requests
- **AppleJuice** — rotating Apple device BLE beacons (AirPods, Beats, AppleTV, etc.)

All four run in a continuous loop, one advertisement per cycle.

---

## Navigation

| Key | Action |
|-----|--------|
| `.` or `Tab` | Next item |
| `;` | Previous item |
| `Enter` or `/` | Select |
| `Esc` or `,` | Back / Home |

---

## Menu structure

```
Boot screen
└── [any key]
    ├── Mobile Devices
    │   ├── BT Maelstrom   ← starts advertising immediately
    │   └── Back
    └── Settings
        ├── Battery Info
        ├── Brightness
        ├── Color Theme
        ├── About
        ├── Reboot
        └── Clear Settings
```

---

## Build & Flash

### Prerequisites

```bash
# arduino-cli
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
sudo mv arduino-cli /usr/local/bin/

# M5Stack board core
arduino-cli core update-index
arduino-cli core install m5stack:esp32

# Required libraries
arduino-cli lib install "M5Cardputer"
arduino-cli lib install "M5Unified"
arduino-cli lib install "M5GFX"
```

### Flash

Plug in the Cardputer via USB-C, find your port:

```bash
arduino-cli board list
# → /dev/ttyACM0  (Linux) or /dev/cu.usbmodem...  (macOS)
```

Add yourself to the `dialout` group (Linux, one-time):

```bash
sudo usermod -a -G dialout $USER
newgrp dialout
```

Compile and flash:

```bash
git clone https://github.com/mfawazsec/cardputer-ct-os
cd cardputer-ct-os
bash scripts/flash-local.sh --device=/dev/ttyACM0
```

The script compiles then flashes in one step. Replace `/dev/ttyACM0` with your actual port.

---

## Boot sequence

1. Watch Dogs logo (~3 seconds)
2. "M5-ctOS" splash with firmware version and key instructions
3. Press any key → main menu

---

## Settings

Settings are persisted in EEPROM across reboots.

| Setting | Default |
|---------|---------|
| Brightness | 100% |
| Dim timeout | 15 seconds |
| Foreground color | Green |
| Background color | Black |

**Clear Settings** restores all defaults.

---

## Credits

- [n0xa/m5stick-nemo](https://github.com/n0xa/m5stick-nemo) — original firmware
- [RapierXbox/ESP32-Sour-Apple](https://github.com/RapierXbox/ESP32-Sour-Apple) — SourApple algorithm (ECTO-1A & WillyJL)
- [M5Stack](https://github.com/m5stack) — Cardputer libraries
- Watch Dogs — Ubisoft (logo used for personal/non-commercial build only)
