# BlauLux v1.0 — Release Notes


---

## Description

First stable release of the BlauLux firmware. BlauLux is the receiver side of the Blau ecosystem: it receives commands from wireless BlauLink buttons via ESP-NOW and controls AC loads (relays, dimmers, LEDs) without requiring a router or central hub.

---

## Features

### Wireless control
- BlauProtocol v1 packet reception via ESP-NOW (channel 1)
- Up to 8 simultaneous BlauLink devices
- Packet deduplication via 2 s window and sequence number
- ACK confirmation for each processed packet

### Output control modes
| Mode | Technology | Parameters |
|------|-----------|-----------|
| On/Off | Relay / MOSFET | State |
| PWM | LEDC 5 kHz 8-bit | Brightness % |
| Triac cycle | AC 50 Hz cycle control | Duty % |
| Triac phase | Phase-cut with ZCD (H11AA4 + MOC3021S) | Delay µs |
| Digital LED | WS2812B 800 kHz | RGB color, brightness % |

### Configuration and interface
- Web configuration portal accessible via Wi-Fi AP (`BlauLux_XXXX`)
- Per-GPIO function assignment (up to 22 pins)
- Predefined hardware templates: `PICO-CLICK`, `SONOFF_BASIC_R4`, `AC_REGULATOR`, `GL-C-309WL`
- Configuration persistence via NVS (Preferences), schema v4

### MQTT / Home Assistant integration
- Asynchronous MQTT client (AsyncMqttClient)
- Home Assistant auto-discovery (light / switch)
- State, command, brightness and RGB topics
- LWT (`offline`) on disconnect

### Physical button
- Short press: toggle the configured output
- Long press (≥ 3 s): enter AP configuration mode
- Click (in AP mode): exit and reboot
- Debounce: 500 ms on press / 200 ms on release

### Visual feedback (NeoPixel LED)
| Color | State |
|-------|-------|
| Green (blink) | Successful boot |
| Purple (soft pulse) | AP mode active |
| Yellow | MQTT connected |
| User color | Normal operation |

### Supported platforms
| File | Platform |
|------|---------|
| `BlauLux_v1.0_esp32c3.bin` | ESP32-C3 (RISC-V, native USB) |
| `BlauLux_v1.0_esp32.bin` | ESP32 (dual-core Xtensa) |
| `BlauLux_v1.0_esp32s3.bin` | ESP32-S3 (dual-core Xtensa, native USB) |
| `BlauLux_v1.0_esp32s2.bin` | ESP32-S2 (single-core, USB OTG) |
| `BlauLux_v1.0_esp32c6.bin` | ESP32-C6 (RISC-V, Wi-Fi 6, native USB) |

---

## Flashing

### Recommended tool: [flash_tool_merge.py](tools/flash_tool_merge.py)

### Initial setup

1. On first boot, the device automatically enters AP mode.
2. Connect to the Wi-Fi network `BlauLux_XXXX` (open).
3. Open `http://192.168.4.1` in your browser.
4. Assign functions to the GPIO pins and save the configuration.
5. The device reboots and enters normal operation mode.

---

## Dependencies (libraries)

| Library | Version |
|---------|---------|
| ESP32Async/AsyncTCP | 3.4.10 |
| ESP32Async/ESPAsyncWebServer | 3.6.0 |
| Adafruit NeoPixel | 1.15.4 |
| marvinroger/AsyncMqttClient | 0.9.0 |

---

## Known limitations

- No OTA (Over-The-Air update) mechanism in this version.
- GPIO limit (22 pins) is hardcoded.
- Only 5 output control modes.
