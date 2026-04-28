# BlauTrigger

**BlauTrigger** is a smart AC load controller built on the ESP32-C3. It acts as the receiver end of the **BlauLink ecosystem** — a battery-powered wireless button sends commands over ESP-NOW, and BlauTrigger controls the connected load in response.

Supports relay switching, digital RGB LEDs (NeoPixel/WS2812), PWM dimming, dual warm/cold white temperature control, and phase-cut triac dimming for AC loads. Configuration is done entirely through a built-in web captive portal — no app required.

---

## Table of Contents

- [Features](#features)
- [Hardware](#hardware)
  - [Hardcoded Configuration](#hardcoded-configuration)
  - [Wiring](#wiring)
- [Getting Started](#getting-started)
  - [Requirements](#requirements)
  - [Build & Flash](#build--flash)
  - [First-Time Setup](#first-time-setup)
- [Configuration](#configuration)
  - [Compile-Time (config.h)](#compile-time-configh)
  - [Runtime (Web UI)](#runtime-web-ui)
- [Usage](#usage)
  - [Physical Button](#physical-button)
  - [Remote Control via BlauLink](#remote-control-via-blaulink)
  - [Web Interface](#web-interface)
- [Control Modes](#control-modes)
- [BlauProtocol](#blauprotocol)
- [Project Structure](#project-structure)
- [Troubleshooting](#troubleshooting)

---

## Features

- **ESP-NOW receiver** — low-latency, connectionless protocol; no router needed
- **5 control modes** — relay, digital RGB LED, single PWM dimmer, dual WW/CW PWM, phase-cut triac dimmer
- **Full BlauProtocol command set** — toggle, on/off, set brightness, set RGB, set CCT, dim up/down, scenes
- **ACK-based reliability** — acknowledges every received command back to the sender
- **Captive portal** — web-based configuration and live testing over WiFi AP
- **WiFi STA + MQTT** — optional home network connection with Home Assistant auto-discovery
- **Physical button** — toggle the load or enter config mode with a long press
- **Persistent configuration** — settings stored in NVS (survives power cycles)
- **Deduplication** — ignores duplicate packets within a 2-second window
- **Multi-language UI** — Catalan, English and Spanish web interface (JS i18n, single file)

---

## Hardware

### Hardcoded Configuration

If `HARDCODED_CONFIG` is defined in `src/config.h`, all hardware parameters are fixed at compile time and the web UI cannot modify them. The configurable parameters are:

- `HW_CONTROL_TYPE` — control mode (0 = relay, 1 = digital LED, 2 = PWM, 3 = WW/CW, 4 = triac)
- `HW_PIN1` — primary output GPIO (relay, NeoPixel data, or PWM channel 1)
- `HW_PIN2` — secondary output GPIO (PWM channel 2 for WW/CW mode)
- `PIN_BOTO` — button input GPIO
- `BRIGHTNESS_DEF` — default brightness percentage (0–100)
- `NUM_LEDS` — number of NeoPixel LEDs

When `HARDCODED_CONFIG` is not defined, all these parameters are read from NVS and can be changed through the web interface.

### Wiring

**PICO_CLICK (default):**
```
GPIO 5  →  Button (pull-up, active LOW)
GPIO 6  →  NeoPixel / WS2812 data line
```

**SONOFF_BASIC_R4:**
```
GPIO 0  →  Onboard button (boot pin, active LOW)
GPIO 4  →  Relay control
GPIO 6  →  Status LED
```

For PWM modes (modes 2 and 3), `pin1` and `pin2` are assigned via the web UI and control LEDC channels at 5 kHz / 8-bit resolution.

**Triac phase control (mode 4):**
```
pin1  →  ZCD input — H11AA4 optocoupler output (active HIGH pulse at zero crossing)
pin2  →  Triac gate — MOC3021S optocoupler input (active HIGH to fire)
pin3  →  WS2812 data line (status LED — amber brightness indicates power level)
```
The firing angle is computed from the configured power level (0–100%). A FreeRTOS task waits for each zero-crossing pulse, delays by `(100 − power%) × 10 ms / 100`, then pulses the MOC3021S for 100 µs. Designed for 50 Hz AC mains.

---

## Getting Started

### Requirements

- [PlatformIO](https://platformio.org/) (CLI or VSCode extension)
- USB-C cable
- ESP32-C3 development board (or compatible hardware)
- USB-to-UART driver if needed (CH340, CP210x)

### Build & Flash

1. Clone the repository:
   ```bash
   git clone https://github.com/your-user/BlauTrigger.git
   cd BlauTrigger/firmware/BlauTrigger
   ```

2. (Optional) Edit `src/config.h` to select your hardware variant.

3. Build and upload:
   ```bash
   pio run -e esp32c3 -t upload
   ```

4. Upload the web UI filesystem:
   ```bash
   pio run -e esp32c3 -t uploadfs
   ```

5. Open the serial monitor to verify boot output:
   ```bash
   pio device monitor -b 115200
   ```

### First-Time Setup

On first boot (or after clearing config), the device detects that no button GPIO is configured and enters WiFi AP mode automatically.

1. Power on the BlauTrigger.
2. On your phone or computer, connect to the WiFi network **`BlauTrigger_XXXX`** (last 4 characters of the MAC address).
3. A captive portal opens automatically — or navigate to `http://192.168.4.1`.
4. Select your control type, assign GPIO pins, and set brightness levels.
5. Press **Save**. The device restarts and enters normal operation.

---

## Configuration

### Compile-Time (`config.h`)

| Macro | Default | Description |
|---|---|---|
| `PICO_CLICK` / `SONOFF_BASIC_R4` | `PICO_CLICK` | Hardware pinout selection |
| `HARDCODED_CONFIG` | *(commented)* | If defined, pins are fixed in code and the web UI cannot change them |
| `CLEAR_CONFIG` | *(commented)* | If defined, clears all NVS on boot. Flash once, then comment out and reflash |
| `WIFI_SSID` | `"BlauTrigger"` | AP name prefix (MAC suffix is appended automatically) |
| `WIFI_PASSWORD` | `""` | AP password — empty for open network |
| `NUM_LEDS` | `1` | Number of NeoPixel LEDs |
| `BRIGHTNESS_DEF` | `15` | Default brightness percentage (0–100) |
| `PWM_FREQ` | `5000` | LEDC PWM frequency in Hz |
| `PWM_RESOLUTION` | `8` | PWM resolution in bits (8 → 0–255 range) |
| `WIFI_AP_HOLD_MS` | `3000` | Button hold duration (ms) to enter config mode |
| `WIFI_AP_TIMEOUT_MS` | `120000` | AP mode auto-timeout before restart |

### Runtime (Web UI)

When `HARDCODED_CONFIG` is not defined, all hardware settings can be changed through the web interface at `http://192.168.4.1`:

- **Control type** — relay / digital LED / PWM / WW-CW / triac
- **GPIO pins** — pin1 (output), pin2 (second output for WW/CW or triac gate), pin3 (WS2812 indicator for triac mode), button pin
- **Brightness** — per control mode, 0–100%
- **WiFi STA** — connect the device to a home router for MQTT support
- **MQTT** — configure broker, credentials, and topic templates
- **Live preview** — test color (RGB) or brightness before saving

---

## Usage

### Physical Button

| Action | Result |
|---|---|
| Short press | Toggle the load on/off |
| Hold 3+ seconds | Enter WiFi AP configuration mode |
| Double press (in AP mode) | Exit AP mode and restart |

The AP mode has an automatic timeout of 2 minutes (`WIFI_AP_TIMEOUT_MS`).

### Remote Control via BlauLink

BlauTrigger listens for ESP-NOW packets from BlauLink sender devices. No pairing or router is required — communication is peer-to-peer at the WiFi MAC layer.

On receiving a valid BlauProtocol packet, BlauTrigger:
1. Validates the CRC-8 checksum.
2. Discards duplicates (same source ID + sequence within 2 seconds).
3. Executes the command (toggle, on, off, set brightness, set color...).
4. Sends an ACK packet back to the sender with the current device state.

### Web Interface

The web interface is always accessible at `http://192.168.4.1` while in AP mode. It exposes the following HTTP endpoints:

| Endpoint | Method | Description |
|---|---|---|
| `/` | `GET` / `POST` | Serve the configuration page / save new hardware config to NVS |
| `/color` | `POST` | Preview RGB color (`r`, `g`, `b` params, 0–255) |
| `/dutty` | `POST` | Preview brightness level (`value`, 0–100) |
| `/duttyCW` | `POST` | Preview cold-white brightness (`value`, 0–100, mode 3/4) |
| `/wifi` | `POST` | Save WiFi STA credentials and reconnect |
| `/mqtt` | `POST` | Save MQTT broker settings and reconnect |
| `/driverMode` | `POST` | Returns current control type (plain text) |
| `/configMode` | `POST` | Returns `"hardcoded"` or `"web"` |
| `/mymac` | `POST` | Returns AP MAC address |
| `/pins` | `POST` | Returns GPIO assignments (JSON) |
| `/brightness` | `POST` | Returns brightness per mode (JSON) |
| `/numLeds` | `POST` | Returns number of NeoPixel LEDs |
| `/boto` | `POST` | Returns button GPIO and pull-up setting (JSON) |
| `/initialSetup` | `POST` | Returns `"true"` if no button GPIO is configured yet |
| `/wifiStatus` | `POST` | Returns WiFi STA connection state (JSON) |
| `/mqttStatus` | `POST` | Returns MQTT connection state and config (JSON) |

---

## Control Modes

| Mode | Value | Description |
|---|---|---|
| On/Off relay | `0` | Binary GPIO switching — relay or digital output |
| Digital LED | `1` | Single NeoPixel/WS2812 with full RGB color support |
| PWM dimmer | `2` | Single LEDC PWM channel for brightness control |
| WW/CW | `3` | Dual LEDC channels for warm/cold white mixing |
| Triac fase | `4` | Phase-cut AC dimmer — H11AA4 ZCD + MOC3021S + WS2812 status LED |

The active mode is stored in NVS and applied on every boot.

---

## BlauProtocol

BlauTrigger uses **BlauProtocol v1** — a compact 10-byte binary protocol designed for ESP-NOW:

```
[ VER | TYPE | SEQ | SRC_ID (2B) | CMD | P1 | P2 | P3 | CRC8 ]
```

| Field | Size | Description |
|---|---|---|
| `VER` | 1 B | Protocol version |
| `TYPE` | 1 B | Message type (event, command, ACK, ping…) |
| `SEQ` | 1 B | Sequence number for deduplication |
| `SRC_ID` | 2 B | Sender device identifier |
| `CMD` | 1 B | Command or event code |
| `P1–P3` | 3 B | Command parameters (brightness, R, G, B…) |
| `CRC8` | 1 B | CRC-8 checksum of bytes 0–8 |

**Message types:** `TYPE_EVENT`, `TYPE_CMD`, `TYPE_ACK`, `TYPE_PING`, `TYPE_PONG`, `TYPE_STATUS_REQ`, `TYPE_STATUS_RSP`

**Event codes:** `EVT_CLICK_1/2/3` (single/double/triple tap), `EVT_LONG_START/END` (long press)

**Command codes:** `CMD_TOGGLE`, `CMD_ON`, `CMD_OFF`, `CMD_SET_BRIGHTNESS`, `CMD_SET_RGB`, `CMD_SET_CCT`, `CMD_SET_SCENE`, `CMD_DIM_UP`, `CMD_DIM_DOWN`

See [`lib/BlauProtocol/blauprotocol.h`](lib/BlauProtocol/blauprotocol.h) for the full specification.

---

## Project Structure

```
BlauTrigger/
├── src/
│   ├── main.cpp          # Application logic, web server, ESP-NOW handler
│   └── config.h          # Hardware pinout & compile-time settings
├── lib/
│   └── BlauProtocol/
│       ├── blauprotocol.h        # Packet structure, types, constants
│       ├── blauprotocol.cpp      # CRC-8, packet init
│       ├── blauprotocol_trg.h    # BlauTrigger helpers: parse, dedup, ACK build
│       └── blauprotocol_link.h   # BlauLink helpers (sender side)
├── data/
│   ├── wifimanager.html   # Multilingual web UI (CA / EN / ES via JS i18n)
│   └── style.css          # Web interface styles
└── platformio.ini         # PlatformIO build configuration
```

---

## Troubleshooting

| Problem | Likely Cause | Solution |
|---|---|---|
| Stuck in AP mode at every boot | Button GPIO not configured | Connect to portal and save pin assignments |
| Can't connect to portal | Captive portal blocked | Navigate manually to `http://192.168.4.1` |
| LED doesn't light | Wrong pin or control type | Verify GPIO in `config.h` or web UI matches hardware |
| No ACK reaching BlauLink | Dedup window expired or packet lost | BlauLink retries within 2 s; check ESP-NOW channel match |
| Config not saving | NVS full or corrupted | Define `CLEAR_CONFIG`, flash once, remove it, reflash |
| Compilation error | Missing library | Run `pio pkg install` to fetch dependencies |
| USB port not detected | Missing USB driver | Install the CH340 or CP210x driver for your OS |

---

## Related Projects

- **[BlauLink](../BlauLink)** — Battery-powered wireless button (sender side of the BlauLink ecosystem)

---

## License

This project is open source. See [LICENSE](LICENSE) for details.
