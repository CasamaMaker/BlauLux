# BlauTrigger AI Collaboration Guide

This document provides the complete context for AI models working on this project. Read it before making any change.

---

## 1. Project Overview & Purpose

**BlauTrigger** is an ESP32-based AC load controller that receives wireless commands from **BlauLink** (battery-powered wireless buttons) and controls lights or relays accordingly.

- **Primary communication:** ESP-NOW (connectionless, no router needed, ~1ms latency)
- **Secondary communication:** WiFi AP + captive portal (configuration)
- **Optional:** WiFi STA + MQTT (home automation integration, e.g. Home Assistant)
- **Target hardware:** ESP32-C3 (PICO_CLICK board) or ESP32 (SONOFF_BASIC_R4)
- **Platform:** Arduino via PlatformIO

BlauTrigger is always the **responder** in the Blau ecosystem. It never initiates communication. BlauLink (a separate device/repository) is always the initiator.

---

## 2. Core Technologies & Stack

| Layer | Technology |
|---|---|
| MCU | ESP32-C3 (primary) / ESP32 (secondary) |
| Framework | Arduino (ESP32 Arduino Core) |
| Build system | PlatformIO (`platformio.ini`) |
| Wireless | ESP-NOW (primary), WiFi AP+STA |
| MQTT | AsyncMqttClient |
| HTTP server | ESPAsyncWebServer + AsyncTCP |
| LED | FastLED (NeoPixel/WS2812) |
| PWM | ESP32 LEDC peripheral |
| Persistence | ESP32 Preferences API (NVS) |
| Filesystem | LittleFS (for web portal HTML/CSS) |
| Protocol | BlauProtocol (shared library in `lib/BlauProtocol/`) |

---

## 3. Repository Layout

```
firmware/BlauTrigger/
├── src/
│   └── main.cpp              — All application logic (1200+ lines)
├── lib/
│   └── BlauProtocol/
│       ├── blauprotocol.h    — Wire format, all constants (TYPE_*, CMD_*, EVT_*, ACK_*)
│       ├── blauprotocol.cpp  — CRC-8, blau_init_packet(), blau_check_crc(), blau_fill_crc()
│       ├── blauprotocol_trg.h — BlauTrigger helpers: parse, dedup, build ACK/PONG/STATUS
│       └── blauprotocol_link.h — BlauLink helpers (sender-side, do not modify for BlauTrigger)
├── data/
│   ├── wifimanager.html      — Captive portal (multilingual: ca/en/es via JS i18n)
│   └── style.css             — Portal styles
├── boards/
│   └── esp32s3_4mb.json      — Custom board definition for ESP32-S3 variant
├── platformio.ini            — Build environments and dependencies
├── BLAUPROTOCOL.md           — Full protocol specification (in Catalan)
├── CODING_GUIDELINES.md      — Developer + AI rules (read this too)
├── README.md                 — Hardware wiring, getting started, control modes
└── docs/
    └── MQTT_HOME_ASSISTANT.md — MQTT/HA Discovery implementation plan
```

**The BlauProtocol library is shared** between BlauTrigger and BlauLink (separate repository). Changes to it must be kept in sync manually.

---

## 4. Control Modes (Hardware Variants)

BlauTrigger supports 5 mutually exclusive control types, set at compile time or via the captive portal:

| Mode | Type | Backend | Use case |
|---|---|---|---|
| 0 | On/Off relay | GPIO + status LED | Binary loads (lamps, fans) |
| 1 | RGB strip | FastLED / NeoPixel | Full-color WS2812 strips |
| 2 | PWM dimmer | LEDC channel 1 | Single-channel dimmable LEDs |
| 3 | WW/CW mixer | LEDC channels 1+2 | Warm + cold white LED mixing |
| 4 | Triac AC | ZCD ISR + FreeRTOS task | Phase-cut dimming of AC loads |

Mode 4 uses:
- **H11AA4** optocoupler: detects zero-crossing of AC mains → triggers ISR
- **MOC3021S** optocoupler: fires the TRIAC gate pulse
- **FreeRTOS task** (`_triacTaskHandle`): waits on `_zcdSemaphore`, calculates firing delay, pulses gate
- Firing delay formula: `delay_us = (100 - power%) * AC_HALF_CYCLE_US / 100`

---

## 5. Architecture & Data Flow

```
[BlauLink button press]
        │  ESP-NOW packet (10 bytes, BlauPacket_t)
        ▼
[OnDataRecv ISR callback]
  - Validate CRC + version
  - Copy packet to _pending_pkt
  - Set _ack_pending = true
  - DO NOT send ACK here
        │
        ▼
[loop()]
  - Detect _ack_pending
  - Call handleAction(pkt)     ← executes command (toggle, brightness, RGB…)
  - Build ACK packet
  - esp_now_send() ACK
  - Clear _ack_pending
```

**Critical constraint:** ACK responses are always sent from `loop()`, never from `OnDataRecv()`. The ESP-NOW callback runs in a Wi-Fi task context where calling `esp_now_send()` causes crashes.

### State Variables

| Variable | Type | Description |
|---|---|---|
| `state` | `bool` | Current on/off state |
| `brightness[5]` | `uint8_t[5]` | Per-mode brightness 0–100% |
| `brightness_cw` | `uint8_t` | Warm-white brightness for modes 3/4 |
| `_ack_pending` | `volatile bool` | True when an ACK needs to be sent from loop() |
| `_ack_mac` | `volatile uint8_t[6]` | Destination MAC for pending ACK |
| `_ack_pkt` | `volatile BlauPacket_t` | ACK packet queued from ISR |
| `_zcdSemaphore` | `SemaphoreHandle_t` | FreeRTOS semaphore for triac ZCD sync |
| `_triacTaskHandle` | `TaskHandle_t` | FreeRTOS task for triac phase control |

---

## 6. BlauProtocol Wire Format (NON-NEGOTIABLE)

```c
typedef struct __attribute__((packed)) {
    uint8_t  version;  // Must be BLAU_PROTO_VERSION (0x01)
    uint8_t  type;     // TYPE_EVENT, TYPE_CMD, TYPE_ACK, TYPE_PING, TYPE_PONG,
                       //   TYPE_STATUS_REQ, TYPE_STATUS_RSP
    uint8_t  seq;      // Circular sequence 0–255, same value across all retries
    uint16_t src_id;   // Last 2 bytes of sender MAC (big-endian)
    uint8_t  cmd;      // CMD_* or EVT_* depending on type; confirmed seq in ACK
    uint8_t  p1;       // Parameter 1 (ACK status code if TYPE_ACK)
    uint8_t  p2;       // Parameter 2
    uint8_t  p3;       // Parameter 3 / flags
    uint8_t  crc8;     // CRC-8, poly 0x07, init 0x00, over bytes 0–8
} BlauPacket_t;
// Total: 10 bytes exactly (BLAU_PACKET_SIZE)
```

### Message Types

| Constant | Value | Direction | Description |
|---|---|---|---|
| `TYPE_EVENT` | 0x01 | Link→Trigger | Button event (click, long press) |
| `TYPE_CMD` | 0x02 | Link→Trigger | Direct command |
| `TYPE_ACK` | 0x03 | Trigger→Link | Execution confirmation |
| `TYPE_PING` | 0x04 | Link→Trigger | Presence check |
| `TYPE_PONG` | 0x05 | Trigger→Link | Ping response |
| `TYPE_STATUS_REQ` | 0x06 | Link→Trigger | Request device status |
| `TYPE_STATUS_RSP` | 0x07 | Trigger→Link | Status response |

### Command Codes (TYPE_CMD)

| Constant | p1 | p2 | p3 |
|---|---|---|---|
| `CMD_TOGGLE` | — | — | — |
| `CMD_ON` | — | — | — |
| `CMD_OFF` | — | — | — |
| `CMD_SET_BRIGHTNESS` | 0–100% | — | — |
| `CMD_SET_RGB` | R (0–255) | G (0–255) | B (0–255) |
| `CMD_SET_CCT` | warm% | cold% | — |
| `CMD_SET_SCENE` | scene_id | — | — |
| `CMD_DIM_UP` | step (1–10) | — | — |
| `CMD_DIM_DOWN` | step (1–10) | — | — |

### Event Codes (TYPE_EVENT)

`EVT_CLICK_1`, `EVT_CLICK_2`, `EVT_CLICK_3`, `EVT_LONG_START`, `EVT_LONG_END`

### ACK Status Codes (p1 in TYPE_ACK)

| Constant | Meaning | Sender treats as |
|---|---|---|
| `ACK_OK` | Executed | Success |
| `ACK_DUPLICATE` | Already executed (dedup) | **Success** |
| `ACK_ERROR` | Execution failed | Retry |
| `ACK_UNAUTHORIZED` | Not paired | Fatal |
| `ACK_BAD_VERSION` | Version mismatch | Fatal |
| `ACK_BAD_CRC` | CRC failed | Retry |

**`ACK_DUPLICATE` MUST be treated as success by BlauLink.** The action was already executed.

### Reliability Mechanism

- **CRC-8:** poly 0x07, init 0x00, computed over bytes 0–8. Always call `blau_fill_crc()` before send; `blau_check_crc()` before processing.
- **Deduplication:** Table of `(src_id, last_seq, timestamp)` for up to `BLAU_MAX_SOURCES` (8) senders. Same `(src_id, seq)` within `BLAU_DEDUP_WINDOW_MS` (2000ms) → `ACK_DUPLICATE`, no re-execution.
- **Retries:** BlauLink sends same packet (same `seq`) up to `BLAU_MAX_RETRIES` (3) times with `BLAU_ACK_TIMEOUT_MS` (50ms) timeout each. BlauTrigger deduplication prevents double execution.

### Adding a New Command

1. Define `CMD_NEW_CMD = 0xXX` in `blauprotocol.h` (no existing value collision).
2. Document `p1`/`p2`/`p3` semantics in a comment next to the constant.
3. Add a `case CMD_NEW_CMD:` in `handleAction()` in `main.cpp`.
4. If unimplemented, respond with `ACK_ERROR` and log `"CMD no implementat: 0x..."`.
5. Update BlauLink's sender if needed (separate repository).

---

## 7. Hardware Variants & Compile-Time Configuration

Hardware is selected via `config.h` macros. **Never add runtime hardware detection.**

```cpp
// config.h defines (mutually exclusive):
#define PICO_CLICK       // Default: ESP32-C3 custom board
#define SONOFF_BASIC_R4  // Sonoff relay module
```

Feature flags (also in `config.h`):

| Flag | Effect |
|---|---|
| `HARDCODED_CONFIG` | Ignores NVS, uses config.h pin/mode values |
| `CLEAR_CONFIG` | Erases NVS on boot (development only) |
| `ENABLE_WIFI_STA` | Connects to home WiFi network |
| `ENABLE_MQTT` | Enables AsyncMqttClient |

**Do not change pin assignments** unless explicitly asked — they are validated against physical PCB layouts.

---

## 8. Coding Conventions

### Naming

| Pattern | Scope |
|---|---|
| `blau_` prefix | BlauProtocol public API functions |
| `BLAU_` prefix | Protocol constants and limits |
| `TYPE_`, `CMD_`, `EVT_`, `ACK_` | Protocol code constants |
| `_ack_*` | Volatile ISR-side ACK state |
| `_*_pending` | Async flags checked in loop() |
| camelCase | Local variables and functions |
| SCREAMING_SNAKE_CASE | Constants and macros |
| PascalCase + `_t` suffix | Structs (e.g. `BlauPacket_t`) |

### Language

- **Comments and UI strings: Catalan.** Do not write in English or Spanish.
- Source identifiers (variable names, function names): English or abbreviated Catalan is acceptable.

### Code Style

- Indentation: 4 spaces. No tabs.
- Always use braces for `if`/`for`/`while`, even single-line bodies.
- Use `const uint8_t` / `constexpr` for typed constants. Reserve `#define` for compile-time feature flags only.
- No magic numbers in protocol logic — always use named constants from `blauprotocol.h`.
- No `String` class in protocol or callback code. Use `char[]` + `snprintf()`.
- Keep functions under ~60 lines.

---

## 9. Embedded Constraints (ESP32-C3 / Arduino)

- **No dynamic allocation** at runtime: no `new`, `malloc`, `std::vector` after `setup()`. Use fixed-size arrays and static buffers.
- **`volatile` on all ISR-shared variables.** Any variable written in `OnDataRecv` or `OnDataSent` and read in `loop()` MUST be `volatile`.
- **No blocking operations in callbacks.** `OnDataRecv` and `OnDataSent` run in the Wi-Fi task. Set flags, copy data — nothing else.
- **No `delay()` in `loop()`.** Use non-blocking timing with `millis()`.
- **PWM:** Use LEDC peripheral (`ledcSetup()`, `ledcAttachPin()`). Do not use `analogWrite()`.
- **`esp_now_send()` is asynchronous.** The return value only indicates queuing success. Actual delivery confirmation comes via `OnDataSent`.
- **`esp_deep_sleep_start()`:** Only called from BlauLink, never from BlauTrigger.
- **LittleFS:** Always check `LittleFS.begin()` return value. Log and return early on failure.
- **Watchdog:** No blocking `while(true)` without `yield()` or `delay()` in loops. FreeRTOS tasks must call `vTaskDelay()` or block on a semaphore.

---

## 10. WiFi & ESP-NOW Channel Constraint

- BlauTrigger always runs in `WIFI_AP_STA` mode. The AP is fixed on **channel 1** for ESP-NOW compatibility.
- ESP32-C3 has a single radio. If STA connects to a router on a different channel, the AP channel will shift and break ESP-NOW from BlauLink devices.
- When enabling `ENABLE_WIFI_STA`, document this constraint clearly.

---

## 11. MQTT & Home Assistant

MQTT support is partially implemented. See `docs/MQTT_HOME_ASSISTANT.md` for the full implementation plan.

When implemented, topics follow this pattern:
```
blautrigger/{id}/state           → "ON" / "OFF"
blautrigger/{id}/brightness      → 0–100
blautrigger/{id}/rgb             → "R,G,B"
blautrigger/{id}/set             ← "ON" / "OFF"
blautrigger/{id}/brightness/set  ← 0–100
blautrigger/{id}/rgb/set         ← "R,G,B"
blautrigger/{id}/available       → "online" / "offline" (LWT)
```

Home Assistant MQTT Discovery publishes to `homeassistant/light/{id}/config` or `homeassistant/switch/{id}/config` depending on control mode.

---

## 12. Development & Testing Workflow

### Building

```bash
pio run -e pico-click          # Build for PICO_CLICK variant
pio run -e sonoff-basic-r4     # Build for Sonoff variant
pio run -e pico-click -t upload # Flash via USB
```

### Serial Debugging

115200 baud. All log lines are prefixed:
```cpp
Serial.printf("[BlauTrigger] OnDataRecv: from %02X:%02X, seq=%d\n", mac[4], mac[5], pkt.seq);
```

Prefix `[BlauTrigger]` for this device. Use `#ifdef DEBUG` guards for verbose paths.

### Smoke Tests After Any Change

1. Flash BlauTrigger → press BlauLink button → output toggles → BlauLink LED turns green.
2. Repeat 5 times quickly → no duplicate executions on BlauTrigger.
3. Power-cycle BlauTrigger → BlauLink can still pair and trigger.
4. Send same `(src_id, seq)` twice within 2 seconds → BlauTrigger sends `ACK_DUPLICATE`, does NOT re-execute.
5. Corrupt a packet byte → BlauTrigger drops it silently (no crash, no ACK).

---

## 13. AI Agent Instructions

### Scope

- **Modify only what is explicitly requested.** Do not refactor surrounding code, rename things, or restructure files.
- **Do not add features** not asked for, even if they seem useful.
- **Do not remove existing comments** unless they are factually wrong.
- **Do not rewrite full files** unless explicitly instructed. Prefer surgical edits.

### Non-Negotiable Protocol Rules

- **Never modify `BlauPacket_t` layout.** It is a wire format. Any change breaks both devices.
- **Never change existing `TYPE_*`, `CMD_*`, `EVT_*`, `ACK_*` numeric values.** Renaming is safe; changing values is not.
- **Always call `blau_fill_crc()` before sending a packet.**
- **Always call `blau_check_crc()` before processing a received packet.**
- **Retries MUST reuse the same `seq` number.**
- **`ACK_DUPLICATE` MUST be treated as success on the sender side.**
- **ACK responses MUST be queued and sent from `loop()`, never from `OnDataRecv()`.**
- **If a change would affect the wire protocol, stop and ask before implementing.**

### Shared Library

BlauProtocol (`lib/BlauProtocol/`) is shared with BlauLink (separate repository at `GitHub/BlauLink/`). If you modify it, explicitly note that the same change must be applied to BlauLink.

### Style Compliance

- Comments in **Catalan**.
- No `new`, `malloc`, `std::vector` in runtime paths.
- No `String` class in protocol or callback code.
- No `delay()` in `loop()`.
- All ISR-shared variables must be `volatile`.
- All numeric protocol literals must use named constants.

### When Uncertain

- Do not guess intent. Leave a `// TODO: [pregunta]` comment and explain the ambiguity.
- Do not assume a pin number — always reference hardware variant macros or ask.
- Do not assume a command is implemented — verify its presence in `handleAction()`.

### Pre-Submit Checklist

- [ ] Only modified what was requested
- [ ] `BlauPacket_t` struct unchanged
- [ ] CRC filled before send, checked on receive
- [ ] Same `seq` used across retries
- [ ] ISR callbacks do no blocking work
- [ ] ACK sent from `loop()`, not from callback
- [ ] No dynamic allocation
- [ ] No magic numbers — named constants used
- [ ] Comments in Catalan
- [ ] If BlauProtocol changed: BlauLink repository must also be updated
