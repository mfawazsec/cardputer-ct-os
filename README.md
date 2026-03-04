# M5-ctOS

Watch Dogs-themed firmware for the **M5Stack Cardputer**. Built on [m5stick-nemo](https://github.com/n0xa/m5stick-nemo) by n0xa — stripped to the essentials and extended with a live scooter telemetry module.

> For authorized security research and educational use only.

---

## Hardware

| | |
|---|---|
| **Device** | M5Stack Cardputer (ESP32-S3) |
| **Display** | 240×135 ST7789 LCD |
| **Input** | Built-in QWERTY keyboard |
| **Wireless** | Wi-Fi 2.4 GHz + BLE 5.0 |

---

## Features

### BT Maelstrom
Cycles all four Bluetooth advertisement types in a continuous loop:
- **SourApple** — randomised Apple proximity pairing popups
- **Swift Pair** — Microsoft Swift Pair device discovery
- **Android Fast Pair** — Google Fast Pair pairing requests
- **AppleJuice** — rotating Apple device beacons (AirPods, Beats, AppleTV, …)

### Wi-Fi Configuration
- Enter SSID and password directly from the Cardputer keyboard
- Credentials stored in NVS — device auto-connects on every boot
- Live status: IP address, RSSI, connection state

### Ather 450X Telemetry
Live dashboard for your Ather scooter via the cloud API:

| Screen | What it shows |
|--------|---------------|
| **Dashboard** | Battery %, range, ride mode, GPS coordinates, last ride stats, API status |
| **Last Ride** | Distance, average speed, max speed |
| **Ride History** | Scrollable cache of the last 20 rides (stored on-device) |
| **Tracker Mode** | Background anti-theft monitor — alerts when scooter moves > 25 m while not charging |
| **Settings** | Email, password, API base URL, vehicle ID (all stored securely in NVS) |
| **Debug** | Live scrolling log of all `[ATHER]` events |

**Anti-theft alert** — full-screen red warning with GPS coordinates, horn flash (`F`), stop (`S`), and acknowledge (`Enter`).

---

## Navigation

| Key | Action |
|-----|--------|
| `.` or `Tab` | Next item |
| `;` | Previous item |
| `Enter` or `/` | Select |
| `,` | Back / Home |

---

## Menu structure

```
Boot screen
└── [any key]
    ├── Mobile Devices
    │   ├── BT Maelstrom
    │   └── Back
    ├── Wi-Fi Config
    │   ├── Connect        ← keyboard entry for SSID + password
    │   ├── Status
    │   ├── Disconnect
    │   └── Back
    ├── Ather
    │   ├── Dashboard      ← live telemetry, refreshes every 5 s
    │   ├── Last Ride
    │   ├── Ride History   ← last 20 rides, scrollable
    │   ├── Tracker Mode   ← toggle anti-theft background monitor
    │   ├── Settings       ← enter API credentials
    │   ├── Debug          ← live API log
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
arduino-cli lib install "ArduinoJson"
```

### Flash

```bash
arduino-cli board list
# → /dev/ttyACM0  (Linux) or /dev/cu.usbmodem...  (macOS)

# Linux: add yourself to dialout group (one-time)
sudo usermod -a -G dialout $USER && newgrp dialout

git clone https://github.com/mfawazsec/cardputer-ct-os
cd cardputer-ct-os
bash scripts/flash-local.sh --device=/dev/ttyACM0
```

---

## First-time setup

### 1. Connect to Wi-Fi
Main Menu → **Wi-Fi Config** → **Connect** → type your hotspot SSID and password.
The device will auto-connect on every subsequent boot.

### 2. Configure Ather API
Main Menu → **Ather** → **Settings** — enter four fields:

| Field | Example |
|-------|---------|
| Email | `you@example.com` |
| Password | your Ather account password |
| API Base URL | `https://app.atherenergy.com` |
| Vehicle ID | your scooter's VIN / vehicle ID |

Press `Enter` after each field. The device attempts login immediately on save.

### 3. Enable Tracker Mode (optional)
Main Menu → **Ather** → **Tracker Mode** → `Enter` to toggle ON.
The background task polls location every 30 s and triggers a full-screen alert if the scooter moves more than 25 m while not charging.

---

## Settings

Persisted in EEPROM/NVS across reboots.

| Setting | Default |
|---------|---------|
| Brightness | 100% |
| Dim timeout | 15 s |
| Foreground color | Green |
| Background color | Black |
| Wi-Fi credentials | — (enter via Wi-Fi Config) |
| Ather credentials | — (enter via Ather → Settings) |

---

## Resource usage

| | |
|---|---|
| Flash | 54% of 3 MB |
| SRAM | 24% of 320 KB |

---

## Credits

- [n0xa/m5stick-nemo](https://github.com/n0xa/m5stick-nemo) — original firmware
- [RapierXbox/ESP32-Sour-Apple](https://github.com/RapierXbox/ESP32-Sour-Apple) — SourApple algorithm (ECTO-1A & WillyJL)
- [M5Stack](https://github.com/m5stack) — Cardputer libraries
- [ArduinoJson](https://arduinojson.org) — JSON parsing
- Watch Dogs — Ubisoft (logo used for personal/non-commercial build only)
