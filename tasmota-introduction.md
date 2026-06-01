# Tasmota GitHub Discussions — Draft Post

---

**Hi Tasmota community 👋**

**TL;DR:** I built a small ESP32 firmware called **Blau** focused on lighting control (buttons + loads), with ESP-NOW as the primary transport instead of MQTT. Just wanted to share it here in case it's interesting to anyone.

---

**What it is**

Blau is two complementary firmwares:

- **BlauClick** — battery-powered ESP32-C3 button (~1 year battery life, no wiring needed)
- **BlauLux** — ESP32-C3 load controller (relay, PWM, AC phase dimmer, NeoPixel/WS2812)

They communicate via **ESP-NOW at the MAC layer** — no router, no broker, <10 ms latency. One BlauLux can handle up to 8 BlauClick buttons simultaneously.

---

**The key design philosophy**

Tasmota is MQTT-first — great ecosystem, great integrations. Blau takes the opposite priority: **the button-to-light link must work independently of any router or server**. MQTT (and Home Assistant auto-discovery) are implemented as a second optional layer, but the core function never depends on network infrastructure.

This is especially relevant for lighting — the light switch should always work, even when the router is rebooting or the broker is down.

The wire protocol is a custom binary format called **BlauProtocol** (10 bytes, CRC-8, sequence-based deduplication) — designed to be minimal and fast over ESP-NOW.

---

**What it supports today:**

- Load types: On/Off, PWM, AC phase dimmer (ZCD + triac), NeoPixel
- Multi-platform: ESP32-C3, ESP32, S3, S2, C6
- Web captive portal for configuration (no app needed)
- MQTT + HA auto-discovery when WiFi is available
- Up to 4 targets per button, 8 buttons per load

---

Not proposing any integration or merge — just sharing in case it sparks interest or inspires something. Happy to discuss the design choices if anyone's curious!

GitHub:
- [BlauClick](https://github.com/CasamaMaker/BlauClick) — the button firmware
- [BlauLux](https://github.com/CasamaMaker/BlauLux) — the load controller firmware
